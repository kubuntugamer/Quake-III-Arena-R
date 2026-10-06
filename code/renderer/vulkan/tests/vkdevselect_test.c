/*
===========================================================================
Test harness for the GPU selection policy in vk_devselect.c.

Build and run (from code/renderer/vulkan):
    cc -std=c99 -Wall -Wextra -Werror \
       vk_devselect.c tests/vkdevselect_test.c -o /tmp/vkdevselect_test
    /tmp/vkdevselect_test

No Vulkan loader, no GPU, no display required. Each case builds a synthetic
device list, calls VK_DevSelect, and checks which device landed in which slot.
===========================================================================
*/
#include "../vk_devselect.h"

#include <stdio.h>
#include <string.h>

static int g_failures = 0;
static int g_checks = 0;

// Shorthand for a discrete GPU with raytracing, the common desktop case.
static vkdevcand_t DiscreteRT(const char *name) {
    vkdevcand_t c;
    memset(&c, 0, sizeof(c));
    c.name = name;
    c.type = VKDEVTYPE_DISCRETE;
    c.deviceLocalMemory = 8ull << 30;
    c.hasGraphicsQueue = 1;
    c.hasComputeQueue = 1;
    c.canPresent = 1;
    c.swapchainAdequate = 1;
    c.requiredExts = 1;
    c.rayTracingExts = 1;
    c.externalMemoryFd = 1;
    c.externalSemaphoreFd = 1;
    c.externalFenceFd = 1;
    return c;
}

// Integrated GPU that presents. This is the desktop-with-no-dGPU case.
static vkdevcand_t IntegratedPresent(const char *name, int rayTracing) {
    vkdevcand_t c;
    memset(&c, 0, sizeof(c));
    c.name = name;
    c.type = VKDEVTYPE_INTEGRATED;
    c.deviceLocalMemory = 512ull << 20;  // shared memory, modest figure
    c.hasGraphicsQueue = 1;
    c.hasComputeQueue = 1;
    c.canPresent = 1;
    c.swapchainAdequate = 1;
    c.requiredExts = 1;
    c.rayTracingExts = rayTracing;
    c.externalMemoryFd = 1;
    c.externalSemaphoreFd = 1;
    c.externalFenceFd = 1;
    return c;
}

// The case this change exists for: on-board iGPU, display owned by the dGPU,
// so there is no queue family with surface support.
static vkdevcand_t IntegratedHeadless(const char *name, int rayTracing) {
    vkdevcand_t c = IntegratedPresent(name, rayTracing);
    c.canPresent = 0;
    return c;
}

static vkdevcand_t CpuRasterizer(const char *name) {
    vkdevcand_t c;
    memset(&c, 0, sizeof(c));
    c.name = name;
    c.type = VKDEVTYPE_CPU;
    c.hasGraphicsQueue = 1;
    c.hasComputeQueue = 1;
    c.canPresent = 1;
    c.swapchainAdequate = 1;
    c.requiredExts = 1;
    c.rayTracingExts = 1;
    return c;
}

static void Check(int condition, const char *what, const char *detail) {
    g_checks++;
    if (condition) {
        printf("  ok    %s\n", what);
    } else {
        printf("  FAIL  %s: %s\n", what, detail);
        g_failures++;
    }
}

// Asserts that device `wantPrimary` is primary and `wantSecondary` is
// secondary, where -1 means "expected to be none". `wantRc` is the expected
// return code: 0 when some device can present, 1 when nothing can.
static void Expect(const char *label, vkdevcand_t *cands, int n,
                   vkmultigpu_t policy, int wantPrimary, int wantSecondary,
                   int wantRc) {
    vkdevselect_t sel;
    printf("%s\n", label);

    const int rc = VK_DevSelect(cands, n, policy, &sel);
    Check(rc == wantRc, "returns expected code",
          wantRc ? "expected a clean failure (no presentable device)"
                 : "expected success");

    if (wantRc == 0) {
        Check(sel.primary == wantPrimary, "primary index", "mismatch");
    } else {
        Check(sel.primary == VK_DEVSELECT_NONE,
              "primary left unset on failure", "should be none");
    }
    Check(sel.secondary == wantSecondary, "secondary index", "mismatch");

    if (sel.primary != VK_DEVSELECT_NONE) {
        printf("        primary   = %s\n", cands[sel.primary].name);
    } else {
        printf("        primary   = (none)\n");
    }
    if (sel.secondary != VK_DEVSELECT_NONE) {
        printf("        secondary = %s\n", cands[sel.secondary].name);
    } else {
        printf("        secondary = (none)\n");
    }
}

// Convenience wrapper for the common "this should work" case.
static void ExpectOk(const char *label, vkdevcand_t *cands, int n,
                     vkmultigpu_t policy, int wantPrimary, int wantSecondary) {
    Expect(label, cands, n, policy, wantPrimary, wantSecondary, 0);
}

// Convenience wrapper for "nothing can present".
static void ExpectNoDevice(const char *label, vkdevcand_t *cands, int n) {
    Expect(label, cands, n, VK_MULTIGPU_AUTO,
           VK_DEVSELECT_NONE, VK_DEVSELECT_NONE, 1);
}

int main(void) {
    printf("vk_devselect policy tests\n\n");

    // ------------------------------------------------------------------
    // Headless on-board iGPU alongside a dGPU that owns the display.
    // This is the case the old present-family gate silently dropped.
    // ------------------------------------------------------------------
    {
        vkdevcand_t c[2];
        c[0] = IntegratedHeadless("Intel Arc iGPU (headless)", 1);
        c[1] = DiscreteRT("NVIDIA discrete");
        ExpectOk("Headless RT iGPU + dGPU: iGPU becomes compute device",
                c, 2, VK_MULTIGPU_AUTO, 1, 0);
    }

    // Pre-Arc Intel has no raytracing, so it must stay out of the compute
    // slot even though it is now allowed to be headless.
    {
        vkdevcand_t c[2];
        c[0] = IntegratedHeadless("Intel UHD (headless, no RT)", 0);
        c[1] = DiscreteRT("NVIDIA discrete");
        ExpectOk("Headless non-RT iGPU + dGPU: no second device",
                c, 2, VK_MULTIGPU_AUTO, 1, VK_DEVSELECT_NONE);
    }

    // The strict policy reproduces the old behaviour, for users who want it.
    {
        vkdevcand_t c[2];
        c[0] = IntegratedHeadless("Intel Arc iGPU (headless)", 1);
        c[1] = DiscreteRT("NVIDIA discrete");
        ExpectOk("STRICT policy refuses a headless second device",
                c, 2, VK_MULTIGPU_STRICT, 1, VK_DEVSELECT_NONE);
    }

    // Disabling multi-GPU entirely.
    {
        vkdevcand_t c[2];
        c[0] = IntegratedPresent("AMD APU", 1);
        c[1] = DiscreteRT("AMD discrete");
        ExpectOk("Policy OFF: no second device",
                c, 2, VK_MULTIGPU_OFF, 1, VK_DEVSELECT_NONE);
    }

    // ------------------------------------------------------------------
    // Headless + RTX weighting. A discrete part without RT must not
    // outrank an integrated part that has it, because the slot only exists
    // for raytracing.
    // ------------------------------------------------------------------
    {
        vkdevcand_t nvidiaNoRT = DiscreteRT("NVIDIA discrete");
        nvidiaNoRT.rayTracingExts = 0;
        nvidiaNoRT.name = "NVIDIA discrete (no RT)";

        vkdevcand_t c[2];
        c[0] = IntegratedHeadless("Intel Arc iGPU (headless, RT)", 1);
        c[1] = nvidiaNoRT;

        // dGPU must still be primary because only it can present, but the
        // headless RT iGPU should win the compute slot on RT weighting.
        ExpectOk("Compute slot weights raytracing above device class",
                c, 2, VK_MULTIGPU_AUTO, 1, 0);
    }

    // ------------------------------------------------------------------
    // Single-GPU cases.
    // ------------------------------------------------------------------
    {
        vkdevcand_t c[1];
        c[0] = IntegratedPresent("AMD Radeon 610M (present, no RT)", 0);
        ExpectOk("Laptop iGPU only, no raytracing",
                c, 1, VK_MULTIGPU_AUTO, 0, VK_DEVSELECT_NONE);
    }

    {
        vkdevcand_t c[1];
        c[0] = IntegratedPresent("AMD Strix Halo (present, RT)", 1);
        ExpectOk("Laptop APU with raytracing, single device",
                c, 1, VK_MULTIGPU_AUTO, 0, VK_DEVSELECT_NONE);
    }

    // ------------------------------------------------------------------
    // Degenerate inputs.
    // ------------------------------------------------------------------
    {
        vkdevcand_t c[1];
        c[0] = CpuRasterizer("llvmpipe");
        ExpectNoDevice("Software rasteriser is never selected", c, 1);
    }

    {
        // A software device must not occupy the compute slot either, even
        // though it advertises RT extensions.
        vkdevcand_t c[2];
        c[0] = DiscreteRT("NVIDIA discrete");
        c[1] = CpuRasterizer("lavapipe");
        ExpectOk("Software rasteriser rejected for the compute slot",
                c, 2, VK_MULTIGPU_AUTO, 0, VK_DEVSELECT_NONE);
    }

    {
        // Headless software device: still no, because CPU is excluded first.
        vkdevcand_t c[2];
        c[0] = DiscreteRT("NVIDIA discrete");
        c[1] = CpuRasterizer("lavapipe (headless)");
        c[1].canPresent = 0;
        ExpectOk("Headless software rasteriser rejected for compute",
                c, 2, VK_MULTIGPU_AUTO, 0, VK_DEVSELECT_NONE);
    }

    {
        vkdevcand_t empty[1];
        ExpectNoDevice("Empty device list is a clean failure", empty, 0);
    }

    {
        // A present-capable device that lacks the required extensions.
        vkdevcand_t c[1];
        c[0] = IntegratedPresent("GPU missing required extensions", 1);
        c[0].requiredExts = 0;
        ExpectNoDevice("Missing required extensions is rejected", c, 1);
    }

    {
        // No swapchain support: cannot present, but could still compute.
        vkdevcand_t c[2];
        c[0] = IntegratedPresent("Intel Arc iGPU (no swapchain)", 1);
        c[0].swapchainAdequate = 0;
        c[0].canPresent = 0;
        c[1] = DiscreteRT("NVIDIA discrete");
        ExpectOk("No-swapchain iGPU is compute-only",
                c, 2, VK_MULTIGPU_AUTO, 1, 0);
    }

    {
        // Three-way: two headless iGPUs with RT, one dGPU. The better
        // integrated part should win the compute slot.
        vkdevcand_t c[3];
        c[0] = IntegratedHeadless("Intel Arc iGPU A", 1);
        c[1] = IntegratedHeadless("AMD Radeon iGPU", 1);
        c[2] = DiscreteRT("NVIDIA discrete");
        ExpectOk("Multiple headless RT iGPUs: first best-ranked wins",
                c, 3, VK_MULTIGPU_AUTO, 2, 0);
    }

    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}