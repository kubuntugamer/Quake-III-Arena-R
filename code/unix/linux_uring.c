/*
** LINUX_URING.C
**
** Minimal dependency-free io_uring file-read backend (raw syscalls, no
** liburing) used to exercise io_uring-capable filesystems/drivers (e.g.
** NVMe P2PDMA / fabric filesystems) from the Quake III asset path.
**
** Stage 1: synchronous pread-via-ring with transparent stdio fallback.
** files.c calls Sys_UringHandleRead() (guarded by fs_useUring); any
** failure or shortfall restores the FILE position and returns false so
** the legacy fread loop retries cleanly.
*/

#include <errno.h>
#include <fcntl.h>
#include <linux/io_uring.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef SYS_io_uring_setup
#error "io_uring syscalls unavailable on this libc/arch"
#endif

#define URING_QD 32

typedef struct {
	int ring_fd;
	unsigned *sq_head;
	unsigned *sq_tail;
	unsigned *sq_mask;
	unsigned *sq_array;
	struct io_uring_sqe *sqes;
	unsigned *cq_head;
	unsigned *cq_tail;
	unsigned *cq_mask;
	struct io_uring_cqe *cqes;
	unsigned sq_entries;
	int ready;
} uring_t;

static uring_t uring;

/* barrier helpers (liburing-style, compiler builtins: no extra deps) */
static unsigned uring_load_acquire(const unsigned *p) {
	unsigned v;
	__atomic_load(p, &v, __ATOMIC_ACQUIRE);
	return v;
}
static void uring_store_release(unsigned *p, unsigned v) {
	__atomic_store(p, &v, __ATOMIC_RELEASE);
}

static long uring_enter(unsigned to_submit, unsigned min_complete) {
	return syscall(SYS_io_uring_enter, uring.ring_fd, to_submit, min_complete,
		IORING_ENTER_GETEVENTS, NULL, 0);
}

/*
================
Sys_UringInit
Try once; returns nonzero when the ring is usable. Safe on kernels
without io_uring (setup fails with ENOSYS -> plain stdio path).
================
*/
static int Sys_UringInit(void) {
	static int tried = 0;
	if (uring.ready) {
		return 1;
	}
	if (tried) {
		return 0;
	}
	tried = 1;
	memset(&uring, 0, sizeof(uring));
	uring.ring_fd = -1;

	struct io_uring_params params;
	memset(&params, 0, sizeof(params));
	long fd = syscall(SYS_io_uring_setup, URING_QD, &params);
	if (fd < 0) {
		return 0;
	}
	uring.ring_fd = (int)fd;

	/* refuse exotic layouts; plain three-mmap setup only */
	if (!(params.features & IORING_FEAT_NODROP)) {
		/* NODROP absent is fine on old kernels; keep going */
	}

	uring.sq_entries = params.sq_entries;
	size_t sq_ring_sz = params.sq_off.array + params.sq_entries * sizeof(unsigned);
	size_t cq_ring_sz = params.cq_off.cqes + params.cq_entries * sizeof(struct io_uring_cqe);
	size_t sqes_sz = params.sq_entries * sizeof(struct io_uring_sqe);

	void *sq_ptr = mmap(0, sq_ring_sz, PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_POPULATE, uring.ring_fd, IORING_OFF_SQ_RING);
	void *cq_ptr = mmap(0, cq_ring_sz, PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_POPULATE, uring.ring_fd, IORING_OFF_CQ_RING);
	if (sq_ptr == MAP_FAILED || cq_ptr == MAP_FAILED) {
		if (sq_ptr != MAP_FAILED) munmap(sq_ptr, sq_ring_sz);
		if (cq_ptr != MAP_FAILED) munmap(cq_ptr, cq_ring_sz);
		close(uring.ring_fd);
		uring.ring_fd = -1;
		return 0;
	}
	/* NOTE: sizes kept in locals via params copy below; store for shutdown */
	uring.sq_head = (unsigned *)((char *)sq_ptr + params.sq_off.head);
	uring.sq_tail = (unsigned *)((char *)sq_ptr + params.sq_off.tail);
	uring.sq_mask = (unsigned *)((char *)sq_ptr + params.sq_off.ring_mask);
	uring.sq_array = (unsigned *)((char *)sq_ptr + params.sq_off.array);
	uring.cq_head = (unsigned *)((char *)cq_ptr + params.cq_off.head);
	uring.cq_tail = (unsigned *)((char *)cq_ptr + params.cq_off.tail);
	uring.cq_mask = (unsigned *)((char *)cq_ptr + params.cq_off.ring_mask);
	uring.cqes = (struct io_uring_cqe *)((char *)cq_ptr + params.cq_off.cqes);
	uring.sqes = mmap(0, sqes_sz, PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_POPULATE, uring.ring_fd, IORING_OFF_SQES);
	if (uring.sqes == MAP_FAILED) {
		munmap(sq_ptr, sq_ring_sz);
		munmap(cq_ptr, cq_ring_sz);
		close(uring.ring_fd);
		uring.ring_fd = -1;
		uring.sqes = NULL;
		return 0;
	}

	uring.ready = 1;
	return 1;
}

/*
================
Sys_UringPReadOnce
Single ring round-trip. Returns bytes (>=0) or -1 on ring/submission error.
================
*/
static long Sys_UringPReadOnce(int fd, void *buf, unsigned len, unsigned long long off) {
	unsigned tail = uring_load_acquire(uring.sq_tail);
	unsigned index = tail & *uring.sq_mask;
	struct io_uring_sqe *sqe = &uring.sqes[index];
	memset(sqe, 0, sizeof(*sqe));
	sqe->opcode = IORING_OP_READ;
	sqe->fd = fd;
	sqe->off = off;
	sqe->addr = (unsigned long long)(uintptr_t)buf;
	sqe->len = len;
	uring.sq_array[index] = index;
	uring_store_release(uring.sq_tail, tail + 1);

	if (uring_enter(1, 1) < 0) {
		return -1;
	}

	unsigned head = uring_load_acquire(uring.cq_head);
	struct io_uring_cqe *cqe = &uring.cqes[head & *uring.cq_mask];
	long res = cqe->res;
	uring_store_release(uring.cq_head, head + 1);
	return res;
}

/*
================
Sys_UringHandleRead
pread() equivalent through the ring for stdio FILE *f.
Returns nonzero only when the full len was transferred; on any
shortfall the file position is restored and 0 is returned so the
caller falls back to the legacy fread loop.
================
*/
int Sys_UringHandleRead(void *vf, void *buf, int len) {
	FILE *f = (FILE *)vf;
	char *p;
	long off;
	int fd;
	long total;

	if (len <= 0) {
		return 1;
	}
	if (!Sys_UringInit()) {
		return 0;
	}
	fd = fileno(f);
	if (fd < 0) {
		return 0;
	}
	off = ftello(f);
	if (off < 0) {
		return 0;
	}

	p = (char *)buf;
	total = 0;
	while (total < len) {
		long r = Sys_UringPReadOnce(fd, p + total, (unsigned)(len - total),
			(unsigned long long)(off + total));
		if (r < 0) {
			break; /* ring/submission error -> fallback */
		}
		if (r == 0) {
			break; /* EOF */
		}
		total += r;
	}

	if (total != len) {
		/* restore position so the fread fallback retries cleanly */
		fseeko(f, off, SEEK_SET);
		return 0;
	}
	/* keep stdio position in sync (also drops any stale read buffer) */
	if (fseeko(f, off + total, SEEK_SET) != 0) {
		return 0;
	}
	return 1;
}

/* ------------------------------------------------------------------
   Stage 2: asynchronous submit/collect batch streaming
   ------------------------------------------------------------------ */
static unsigned async_inflight;
static unsigned long long async_next_tag = 1;

/*
================
Sys_UringReadAsync
Non blocking: queue one pread of *len* bytes from the open file *vf*
at byte offset *off*, landing in *buf*. Returns 1 on successful
submission, 0 when the ring is unavailable, busy, or any setup step
fails. The completion is later reported through Sys_UringCollect.
================
*/
int Sys_UringReadAsync(void *vf, void *buf, int len, long off, unsigned long long tag) {
	FILE *f = (FILE *)vf;
	int fd;
	unsigned tail, index;
	struct io_uring_sqe *sqe;

	if (len <= 0) {
		return 0;
	}
	if (!Sys_UringInit()) {
		return 0;
	}
	if (async_inflight >= uring.sq_entries) {
		return 0; /* too many in flight; caller drains first */
	}
	fd = fileno(f);
	if (fd < 0) {
		return 0;
	}

	tail = uring_load_acquire(uring.sq_tail);
	index = tail & *uring.sq_mask;
	sqe = &uring.sqes[index];
	memset(sqe, 0, sizeof(*sqe));
	sqe->opcode = IORING_OP_READ;
	sqe->fd = fd;
	sqe->off = (unsigned long long)off;
	sqe->addr = (unsigned long long)(uintptr_t)buf;
	sqe->len = (unsigned)len;
	sqe->user_data = tag;
	uring.sq_array[index] = index;
	uring_store_release(uring.sq_tail, tail + 1);

	if (uring_enter(1, 0) < 0) {
		return 0;
	}
	async_inflight++;
	return 1;
}

/*
================
Sys_UringCollect
Reap completed async reads. When *block* is nonzero, wait for at
least one completion first; otherwise drain whatever is available
without blocking. Fills *tags* (max *maxout*) with caller tags and
*results* with byte counts (or a negative errno). Returns the number
of completions collected. In-flight bookkeeping is updated accordingly.
================
*/
int Sys_UringCollect(int block, unsigned long long *tags, long *results, int maxout) {
	unsigned head, avail;
	int collected = 0;

	if (!Sys_UringInit()) {
		return 0;
	}
	if (uring_enter(0, block) < 0) {
		return 0;
	}
	head = uring_load_acquire(uring.cq_head);
	avail = uring_load_acquire(uring.cq_tail) - head;
	while (collected < maxout && avail > 0) {
		struct io_uring_cqe *cqe = &uring.cqes[head & *uring.cq_mask];
		if (tags) {
			tags[collected] = (unsigned long long)cqe->user_data;
		}
		if (results) {
			results[collected] = cqe->res;
		}
		collected++;
		head++;
		avail--;
		if (async_inflight > 0) {
			async_inflight--;
		}
	}
	uring_store_release(uring.cq_head, head);
	return collected;
}

/*
================
Sys_UringHandleReadAsync
Exact-length read routed through the async submit/collect substrate.
On success the stdio position advances by *len* and 1 is returned.
On any shortfall/error the file position is restored and 0 is
returned so the caller retries via the legacy fread loop.
================
*/
int Sys_UringHandleReadAsync(void *vf, void *buf, int len) {
	FILE *f = (FILE *)vf;
	char *p;
	long off;
	unsigned long long tag;
	long total;
	int fd;

	if (len <= 0) {
		return 1;
	}
	if (!Sys_UringInit()) {
		return 0;
	}
	fd = fileno(f);
	if (fd < 0) {
		return 0;
	}
	off = ftello(f);
	if (off < 0) {
		return 0;
	}

	p = (char *)buf;
	total = 0;
	tag = async_next_tag++;
	if (!Sys_UringReadAsync(vf, buf, len, off, tag)) {
		return 0; /* ring busy or unavailable -> fread fallback */
	}

	/* drain completions until our tag arrives (other work-tag drains
	   from a shared ring are rare in Q3's single read thread) */
	for (;;) {
		unsigned long long rtags[URING_QD];
		long rres[URING_QD];
		int got = Sys_UringCollect(1, rtags, rres, URING_QD);
		int i;

		if (got <= 0) {
			return 0;
		}
		for (i = 0; i < got; i++) {
			if (rtags[i] == tag) {
				if (rres[i] != len) {
					/* shortfall or error -> restore position, fallback */
					fseeko(f, off, SEEK_SET);
					return 0;
				}
				total = rres[i];
				if (fseeko(f, off + total, SEEK_SET) != 0) {
					return 0;
				}
				return 1;
			}
		}
		/* our tag not in this drain; loop to wait for the rest */
	}
}
