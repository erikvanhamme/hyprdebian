// The tests of the choice of the Vulkan device, common/src/gpu_pick.cpp:
// lists of devices, as gpu.cpp describes them, with and without the main
// device of the dmabuf feedback of the compositor. They check that the GPU
// of the compositor comes first, the order of the kinds without feedback or
// without a match, which devices will do on Vulkan 1.2 and 1.3, and the three
// devices of hyprlap.
//
// make test in the package directory compiles and runs it; it prints a line
// for every check that fails and exits with 1 when one did.

#include "gpu_pick.hpp"

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace {

int failures = 0;

/** @brief Counts and prints a check that fails. */
void check(bool ok, const std::string& what)
{
    if (!ok) {
        failures++;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

/** @brief Checks that pick_device() takes the device of index want, or none. */
void check_pick(const std::vector<DeviceInfo>& devices, std::optional<DrmNode> compositor,
                std::optional<std::size_t> want, const std::string& what)
{
    const std::optional<std::size_t> got = pick_device(devices, compositor);
    check(got == want, what + ": takes " + (want ? devices[*want].name : std::string("none")) + ", not " +
                           (got ? devices[*got].name : std::string("none")));
}

/** @brief A device of Vulkan 1.3 that will do, of a kind, with a primary and a render node. */
DeviceInfo device13(const std::string& name, DeviceKind kind, int64_t primary_minor, int64_t render_minor)
{
    return {
        .name = name,
        .version = {1, 3},
        .kind = kind,
        .presents = true,
        .swapchain = true,
        .primary = DrmNode{226, primary_minor},
        .render = DrmNode{226, render_minor},
    };
}

/** @brief lavapipe or llvmpipe: Vulkan 1.4 on the CPU, without DRM nodes. */
DeviceInfo llvmpipe()
{
    return {
        .name = "llvmpipe",
        .version = {1, 4},
        .kind = DeviceKind::cpu,
        .presents = true,
        .swapchain = true,
        .dynamic_rendering = true,
        .synchronization2 = true,
    };
}

/** @brief The order of the kinds, without feedback. */
void test_order()
{
    const DeviceInfo discrete = device13("discrete", DeviceKind::discrete, 0, 128);
    const DeviceInfo integrated = device13("integrated", DeviceKind::integrated, 1, 129);
    DeviceInfo virtual_gpu = device13("virtual", DeviceKind::virtual_gpu, 2, 130);
    DeviceInfo other = device13("other", DeviceKind::other, 3, 131);

    check_pick({llvmpipe(), integrated, discrete}, std::nullopt, 2, "order: discrete before integrated and CPU");
    check_pick({llvmpipe(), integrated}, std::nullopt, 1, "order: integrated before CPU");
    check_pick({llvmpipe(), virtual_gpu}, std::nullopt, 1, "order: virtual before CPU");
    check_pick({virtual_gpu, integrated}, std::nullopt, 1, "order: integrated before virtual");
    check_pick({other, llvmpipe()}, std::nullopt, 1, "order: CPU before another kind");
    check_pick({other}, std::nullopt, 0, "order: another kind when it is all there is");
    check_pick({llvmpipe()}, std::nullopt, 0, "order: the CPU when it is all there is");
    check_pick({}, std::nullopt, std::nullopt, "order: no devices");

    DeviceInfo first = device13("first discrete", DeviceKind::discrete, 0, 128);
    DeviceInfo second = device13("second discrete", DeviceKind::discrete, 1, 129);
    check_pick({first, second}, std::nullopt, 0, "order: the first of two of a kind");
}

/** @brief Which devices will do. */
void test_usable()
{
    DeviceInfo d = device13("1.3", DeviceKind::discrete, 0, 128);
    check(usable(d) && core_features(d), "usable: 1.3 without the extensions, with the core features");

    d.version = {1, 4};
    check(usable(d) && core_features(d), "usable: 1.4, with the core features");

    d.version = {1, 2};
    check(!usable(d), "usable: not 1.2 without the extensions");
    d.dynamic_rendering = true;
    check(!usable(d), "usable: not 1.2 with only VK_KHR_dynamic_rendering");
    d.dynamic_rendering = false;
    d.synchronization2 = true;
    check(!usable(d), "usable: not 1.2 with only VK_KHR_synchronization2");
    d.dynamic_rendering = true;
    check(usable(d) && !core_features(d), "usable: 1.2 with both extensions, through the extensions");

    d.version = {1, 1};
    check(!usable(d), "usable: not 1.1, also with both extensions");

    d = device13("no swapchain", DeviceKind::discrete, 0, 128);
    d.swapchain = false;
    check(!usable(d), "usable: not without VK_KHR_swapchain");

    d = device13("no present", DeviceKind::discrete, 0, 128);
    d.presents = false;
    check(!usable(d), "usable: not when it cannot present to the compositor");

    DeviceInfo old = device13("discrete 1.2", DeviceKind::discrete, 0, 128);
    old.version = {1, 2};
    check_pick({old, llvmpipe()}, std::nullopt, 1, "usable: a 1.2 device without the extensions is passed over");
    old.dynamic_rendering = true;
    old.synchronization2 = true;
    check_pick({llvmpipe(), old}, std::nullopt, 1, "usable: a 1.2 device with both extensions is taken");
}

/** @brief The GPU of the compositor first. */
void test_compositor()
{
    const DeviceInfo discrete = device13("discrete", DeviceKind::discrete, 0, 128);
    const DeviceInfo integrated = device13("integrated", DeviceKind::integrated, 1, 129);
    const std::vector<DeviceInfo> both = {discrete, integrated};

    check_pick(both, DrmNode{226, 1}, 1, "compositor: its primary node over a discrete GPU");
    check_pick(both, DrmNode{226, 129}, 1, "compositor: its render node over a discrete GPU");
    check_pick(both, DrmNode{226, 0}, 0, "compositor: the discrete GPU when it drives the compositor");
    check_pick(both, DrmNode{226, 2}, 0, "compositor: the order without a match");
    check_pick(both, DrmNode{227, 1}, 0, "compositor: the major number counts too");
    check_pick(both, std::nullopt, 0, "compositor: the order without feedback");
    check_pick({llvmpipe()}, DrmNode{226, 128}, 0, "compositor: the CPU when no device has its node");

    DeviceInfo unusable = integrated;
    unusable.version = {1, 2};
    check_pick({discrete, unusable}, DrmNode{226, 129}, 0, "compositor: the order when its GPU will not do");

    DeviceInfo no_drm = integrated;
    no_drm.primary.reset();
    no_drm.render.reset();
    check_pick({discrete, no_drm}, DrmNode{226, 129}, 0, "compositor: the order when its GPU has no DRM numbers");
}

/** @brief hyprlap of the spec: the HD 4000 on hasvk, the Quadro K3000M on NVK, and llvmpipe. */
void test_hyprlap()
{
    const DeviceInfo hd4000 = {
        .name = "Intel(R) HD Graphics 4000 (IVB GT2)",
        .version = {1, 2},
        .kind = DeviceKind::integrated,
        .presents = true,
        .swapchain = true,
        .dynamic_rendering = true,
        .synchronization2 = true,
        .primary = DrmNode{226, 1},
        .render = DrmNode{226, 129},
    };
    const DeviceInfo k3000m = {
        .name = "NVK GK104",
        .version = {1, 2},
        .kind = DeviceKind::discrete,
        .presents = true,
        .swapchain = true,
        .dynamic_rendering = true,
        .synchronization2 = true,
        .primary = DrmNode{226, 0},
        .render = DrmNode{226, 128},
    };
    const std::vector<DeviceInfo> devices = {k3000m, hd4000, llvmpipe()};

    check_pick(devices, DrmNode{226, 1}, 1, "hyprlap: the HD 4000 by its primary node");
    check_pick(devices, DrmNode{226, 129}, 1, "hyprlap: the HD 4000 by its render node");
    check_pick(devices, std::nullopt, 0, "hyprlap: the Kepler without feedback, as the order of the kinds");
    check(!core_features(devices[1]), "hyprlap: the HD 4000 through the extensions");
    check(core_features(devices[2]), "hyprlap: llvmpipe with the core features");

    // Before Vulkan 1.2 did: only llvmpipe has 1.3.
    std::vector<DeviceInfo> without = devices;
    without[0].dynamic_rendering = false;
    without[1].dynamic_rendering = false;
    check_pick(without, DrmNode{226, 129}, 2, "hyprlap: llvmpipe when the GPUs lack the extensions");
}

} // namespace

int main()
{
    test_order();
    test_usable();
    test_compositor();
    test_hyprlap();

    if (failures > 0) {
        std::printf("%d checks failed\n", failures);
        return 1;
    }
    std::printf("all checks passed\n");
    return 0;
}
