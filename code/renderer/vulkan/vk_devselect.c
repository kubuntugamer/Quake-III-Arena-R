/*
===========================================================================
GPU selection policy - implementation. See vk_devselect.h for rationale.
===========================================================================
*/
#include "vk_devselect.h"

#include <stddef.h>

int VK_DevCanPresent(const vkdevcand_t *c) {
    // A software rasteriser can technically present, but it is never a useful
    // choice for a game, so it is excluded here rather than ranked.
    if (c->type == VKDEVTYPE_CPU) {
        return 0;
    }
    return c->hasGraphicsQueue && c->canPresent && c->swapchainAdequate &&
           c->requiredExts;
}

int VK_DevCanCompute(const vkdevcand_t *c) {
    // Note the absence of any present check. This is the whole point: on a
    // muxless hybrid the on-board iGPU cannot present to our surface, and
    // gating compute on that would make it unusable for its intended job.
    if (c->type == VKDEVTYPE_CPU) {
        return 0;
    }
    if (!c->rayTracingExts) {
        return 0;
    }
    // Compute or graphics both satisfy a compute submission. Integrated parts
    // often expose a single graphics+compute family and nothing else.
    return (c->hasComputeQueue || c->hasGraphicsQueue) && c->requiredExts;
}

// Shared-memory reporting on integrated GPUs is inconsistent: some drivers
// report a modest device-local heap, some report the whole system RAM. Either
// way it is not a useful tiebreaker, so bucket it and do not lean on it.
static int MemRank(uint64_t mem) {
    if (mem > (1ull << 32)) {
        return 2;
    }
    return mem > 0 ? 1 : 0;
}

static int TypeRank(vkdevtype_t t) {
    switch (t) {
        case VKDEVTYPE_DISCRETE:   return 3;
        case VKDEVTYPE_INTEGRATED: return 2;
        case VKDEVTYPE_VIRTUAL:    return 1;
        default:                   return 0;
    }
}

int VK_DevRank(const vkdevcand_t *c, int forCompute) {
    const int typeRank = TypeRank(c->type);

    if (forCompute) {
        // Weight raytracing support above device class. A discrete GPU without
        // RT extensions should not outrank an integrated GPU that has them,
        // because the only thing this slot is for is raytracing compute.
        const int rtBonus = c->rayTracingExts ? 4 : 0;

        // Tie-break on external-memory support: cross-device sharing needs it,
        // and without it a multi-GPU split cannot hand work across.
        const int xdevBonus = c->externalMemoryFd ? 1 : 0;

        return typeRank * 8 + rtBonus + xdevBonus;
    }

    return typeRank * 4 + MemRank(c->deviceLocalMemory);
}

int VK_DevSelect(const vkdevcand_t *cands, int count, vkmultigpu_t policy,
                 vkdevselect_t *out) {
    out->primary = VK_DEVSELECT_NONE;
    out->secondary = VK_DEVSELECT_NONE;

    if (cands == NULL || count <= 0) {
        return 1;
    }

    int bestPresent = VK_DEVSELECT_NONE;
    int bestPresentRank = -1;

    for (int i = 0; i < count; ++i) {
        if (!VK_DevCanPresent(&cands[i])) {
            continue;
        }
        const int rank = VK_DevRank(&cands[i], 0);
        if (rank > bestPresentRank) {
            bestPresentRank = rank;
            bestPresent = i;
        }
    }

    if (bestPresent == VK_DEVSELECT_NONE) {
        return 1;
    }
    out->primary = bestPresent;

    if (policy == VK_MULTIGPU_OFF) {
        return 0;
    }

    int bestCompute = VK_DEVSELECT_NONE;
    int bestComputeRank = -1;

    for (int i = 0; i < count; ++i) {
        // Never let a device occupy both slots.
        if (i == bestPresent) {
            continue;
        }
        if (!VK_DevCanCompute(&cands[i])) {
            continue;
        }
        // STRICT is the old behaviour: refuse to use a GPU that cannot
        // present, which is what silently excluded headless iGPUs.
        if (policy == VK_MULTIGPU_STRICT && !VK_DevCanPresent(&cands[i])) {
            continue;
        }

        const int rank = VK_DevRank(&cands[i], 1);
        if (rank > bestComputeRank) {
            bestComputeRank = rank;
            bestCompute = i;
        }
    }

    out->secondary = bestCompute;
    return 0;
}