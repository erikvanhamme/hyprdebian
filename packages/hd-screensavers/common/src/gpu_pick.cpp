#include "gpu_pick.hpp"

namespace {

/**
 * @brief The rank of a kind of device: the lowest is taken first.
 * @param kind The kind of the device.
 * @return 0 for a discrete GPU, then integrated, virtual and CPU; 4 for others.
 */
int rank(DeviceKind kind)
{
    switch (kind) {
    case DeviceKind::discrete:
        return 0;
    case DeviceKind::integrated:
        return 1;
    case DeviceKind::virtual_gpu:
        return 2;
    case DeviceKind::cpu:
        return 3;
    case DeviceKind::other:
        break;
    }
    return 4;
}

/** @brief Whether a device has a node, as its primary or its render node. */
bool has_node(const DeviceInfo& device, const DrmNode& node)
{
    return device.primary == node || device.render == node;
}

} // namespace

bool core_features(const DeviceInfo& device)
{
    return device.version >= VulkanVersion{1, 3};
}

bool usable(const DeviceInfo& device)
{
    if (!device.presents || !device.swapchain) {
        return false;
    }
    if (core_features(device)) {
        return true;
    }
    return device.version >= VulkanVersion{1, 2} && device.dynamic_rendering && device.synchronization2;
}

std::optional<std::size_t> pick_device(const std::vector<DeviceInfo>& devices, std::optional<DrmNode> compositor)
{
    if (compositor.has_value()) {
        for (std::size_t i = 0; i < devices.size(); ++i) {
            if (usable(devices[i]) && has_node(devices[i], *compositor)) {
                return i;
            }
        }
    }

    std::optional<std::size_t> best;
    for (std::size_t i = 0; i < devices.size(); ++i) {
        if (!usable(devices[i])) {
            continue;
        }
        if (!best.has_value() || rank(devices[i].kind) < rank(devices[*best].kind)) {
            best = i;
        }
    }
    return best;
}
