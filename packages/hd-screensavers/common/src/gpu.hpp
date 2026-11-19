/**
 * @file gpu.hpp
 * @brief The Vulkan device that renders on every output.
 *
 * One instance, one device and one queue for all the surfaces of the
 * screensaver. The device is the first discrete GPU that can present to the
 * Wayland compositor, else an integrated one, else a virtual one (as in a
 * vm), else one that renders on the CPU, as lavapipe of mesa-vulkan-drivers.
 * It needs Vulkan 1.3, for dynamic rendering and synchronization2, and a
 * queue that draws and presents.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vulkan/vulkan.h>

struct wl_display;

/** @brief The instance, the device, its queue and what the device can do. */
class Gpu {
public:
    /**
     * @brief Creates the instance and the device.
     * @param display The connection to the compositor the device must present to.
     * @param msaa    The most samples per pixel; 1, 2, 4 or 8 are taken, the most the device has up to it.
     * @throws std::runtime_error When there is no Vulkan, or no device that
     *         has Vulkan 1.3 and can present to the compositor.
     */
    Gpu(wl_display* display, int msaa);

    /** @brief Waits until the device is idle and destroys it and the instance. */
    ~Gpu();

    Gpu(const Gpu&) = delete;
    Gpu& operator=(const Gpu&) = delete;

    /** @brief The instance, for the surfaces. */
    VkInstance instance() const { return instance_; }

    /** @brief The physical device, for the surface formats and memory types. */
    VkPhysicalDevice physical_device() const { return physical_device_; }

    /** @brief The device. */
    VkDevice device() const { return device_; }

    /** @brief The queue that draws and presents. */
    VkQueue queue() const { return queue_; }

    /** @brief The family of queue(). */
    uint32_t queue_family() const { return queue_family_; }

    /**
     * @brief The samples per pixel of the colour and depth attachments.
     *
     * The most that both colour and depth attachments have on the device, up
     * to the msaa of the constructor; 1 on a device without multisampling.
     */
    VkSampleCountFlagBits samples() const { return samples_; }

    /** @brief The format of the depth attachments. */
    VkFormat depth_format() const { return depth_format_; }

    /** @brief Whether the device shades every sample, sampleRateShading, which is enabled when it has it. */
    bool sample_rate_shading() const { return sample_rate_shading_; }

    /** @brief The name of the device, for the messages. */
    const std::string& name() const { return name_; }

    /**
     * @brief Finds a memory type of the device.
     * @param bits       The memory types that the resource can be in, from its VkMemoryRequirements.
     * @param properties What the memory type must have.
     * @return The index of the first memory type in bits with properties.
     * @throws std::runtime_error When there is none.
     */
    uint32_t memory_type(uint32_t bits, VkMemoryPropertyFlags properties) const;

private:
    /**
     * @brief Picks the physical device, its queue family and its sample count.
     * @param display The connection to the compositor.
     * @param msaa    The most samples per pixel.
     * @throws std::runtime_error When no device fits.
     */
    void pick_device(wl_display* display, int msaa);

    /** @brief The instance. */
    VkInstance instance_ = VK_NULL_HANDLE;
    /** @brief The physical device that pick_device() chose. */
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    /** @brief The device. */
    VkDevice device_ = VK_NULL_HANDLE;
    /** @brief The queue that draws and presents. */
    VkQueue queue_ = VK_NULL_HANDLE;
    /** @brief The family of queue_. */
    uint32_t queue_family_ = 0;
    /** @brief The samples per pixel. */
    VkSampleCountFlagBits samples_ = VK_SAMPLE_COUNT_1_BIT;
    /** @brief The format of the depth attachments. */
    VkFormat depth_format_ = VK_FORMAT_UNDEFINED;
    /** @brief Whether sampleRateShading is enabled. */
    bool sample_rate_shading_ = false;
    /** @brief The name of the device. */
    std::string name_;
};

/**
 * @brief Throws when a Vulkan call fails.
 * @param result The result of the call.
 * @param what   What the call does, for the message: "cannot <what>".
 * @throws std::runtime_error When result is an error.
 */
void check(VkResult result, const char* what);
