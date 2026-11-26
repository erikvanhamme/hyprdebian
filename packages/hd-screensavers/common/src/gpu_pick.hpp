/**
 * @file gpu_pick.hpp
 * @brief The choice of the Vulkan device, without Wayland or Vulkan.
 *
 * gpu.cpp describes every physical device with a DeviceInfo and lets
 * pick_device() choose, so that the choice can be tested with lists of
 * devices.
 *
 * A device will do when it can present to the compositor, has
 * VK_KHR_swapchain and has dynamic rendering and synchronization2: in the
 * core of Vulkan 1.3 and later, or on Vulkan 1.2 as the extensions
 * VK_KHR_dynamic_rendering and VK_KHR_synchronization2. Of those, the device
 * that drives the compositor comes first: the one whose primary or render
 * node is the main device of the dmabuf feedback of the compositor, by the
 * DRM numbers of VK_EXT_physical_device_drm. Without feedback, or when no
 * device that will do has that node, it is the first discrete GPU, else an
 * integrated one, else a virtual one, else one that renders on the CPU.
 */
#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/** @brief A DRM device node, by its major and minor numbers, as 226:128. */
struct DrmNode {
    /** @brief The major number. */
    int64_t major = 0;
    /** @brief The minor number. */
    int64_t minor = 0;

    bool operator==(const DrmNode&) const = default;
};

/** @brief The kind of a device, as VkPhysicalDeviceType. */
enum class DeviceKind { discrete, integrated, virtual_gpu, cpu, other };

/** @brief A version of Vulkan, as 1.3. */
struct VulkanVersion {
    /** @brief The major number. */
    uint32_t major = 0;
    /** @brief The minor number. */
    uint32_t minor = 0;

    auto operator<=>(const VulkanVersion&) const = default;
};

/** @brief What the choice needs to know of a physical device. */
struct DeviceInfo {
    /** @brief The name of the device, for the messages. */
    std::string name;
    /** @brief The version of Vulkan the device has. */
    VulkanVersion version;
    /** @brief The kind of the device. */
    DeviceKind kind = DeviceKind::other;
    /** @brief Whether a queue family of it draws and presents to the compositor. */
    bool presents = false;
    /** @brief Whether it has VK_KHR_swapchain. */
    bool swapchain = false;
    /** @brief Whether it has VK_KHR_dynamic_rendering. */
    bool dynamic_rendering = false;
    /** @brief Whether it has VK_KHR_synchronization2. */
    bool synchronization2 = false;
    /** @brief Its primary node, from VK_EXT_physical_device_drm; none without it. */
    std::optional<DrmNode> primary;
    /** @brief Its render node, from VK_EXT_physical_device_drm; none without it. */
    std::optional<DrmNode> render;
};

/**
 * @brief Whether a device has dynamic rendering and synchronization2 in its core, Vulkan 1.3 and later.
 *
 * On such a device they are turned on with VkPhysicalDeviceVulkan13Features;
 * on Vulkan 1.2 with the extensions and their structs.
 */
bool core_features(const DeviceInfo& device);

/**
 * @brief Whether the screensavers can draw with a device.
 * @return Whether it presents, has VK_KHR_swapchain, and has Vulkan 1.3, or
 *         Vulkan 1.2 with VK_KHR_dynamic_rendering and VK_KHR_synchronization2.
 */
bool usable(const DeviceInfo& device);

/**
 * @brief Chooses the device to draw with.
 * @param devices    The physical devices, in the order of Vulkan.
 * @param compositor The main device of the dmabuf feedback of the
 *                   compositor; none when it gave no feedback.
 * @return The index in devices of the usable device whose primary or render
 *         node is compositor; else of the first usable one of the best kind:
 *         discrete, integrated, virtual, CPU, other. None when no device is
 *         usable.
 */
std::optional<std::size_t> pick_device(const std::vector<DeviceInfo>& devices, std::optional<DrmNode> compositor);
