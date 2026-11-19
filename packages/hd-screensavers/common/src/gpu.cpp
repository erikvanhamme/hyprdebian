#include "gpu.hpp"

#include "program.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

void check(VkResult result, const char* what)
{
    if (result < 0) {
        throw std::runtime_error(std::string("cannot ") + what + " (VkResult " + std::to_string(result) + ")");
    }
}

namespace {

/**
 * @brief The rank of a kind of device: the lowest is taken first.
 * @param type The kind of the device.
 * @return 0 for a discrete GPU, then integrated, virtual and CPU; 4 for others.
 */
int rank(VkPhysicalDeviceType type)
{
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return 0;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return 1;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return 2;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        return 3;
    default:
        return 4;
    }
}

/**
 * @brief Whether a device has an extension.
 * @param device The physical device.
 * @param name   The name of the extension.
 */
bool has_extension(VkPhysicalDevice device, const char* name)
{
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> extensions(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data());
    return std::any_of(extensions.begin(), extensions.end(),
                       [name](const VkExtensionProperties& e) { return std::strcmp(e.extensionName, name) == 0; });
}

/**
 * @brief The queue family of a device that draws and presents to the compositor.
 * @param device  The physical device.
 * @param display The connection to the compositor.
 * @return The index of the first such family; -1 when there is none.
 */
int64_t draw_and_present_family(VkPhysicalDevice device, wl_display* display)
{
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());
    for (uint32_t i = 0; i < count; ++i) {
        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 &&
            vkGetPhysicalDeviceWaylandPresentationSupportKHR(device, i, display)) {
            return i;
        }
    }
    return -1;
}

} // namespace

Gpu::Gpu(wl_display* display)
{
    const VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = PROGRAM,
        .apiVersion = VK_API_VERSION_1_3,
    };
    const char* const instance_extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME};
    const VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app,
        .enabledExtensionCount = 2,
        .ppEnabledExtensionNames = instance_extensions,
    };
    // Without a driver, the loader has no instance with the Wayland surfaces.
    check(vkCreateInstance(&instance_info, nullptr, &instance_), "create the Vulkan instance: no Vulkan driver");

    pick_device(display);

    VkPhysicalDeviceFeatures supported = {};
    vkGetPhysicalDeviceFeatures(physical_device_, &supported);
    sample_rate_shading_ = supported.sampleRateShading == VK_TRUE;

    // Dynamic rendering and synchronization2 are in every device of Vulkan
    // 1.3, but they have to be enabled.
    VkPhysicalDeviceVulkan13Features features13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
    };
    VkPhysicalDeviceFeatures2 features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &features13,
    };
    features.features.sampleRateShading = sample_rate_shading_ ? VK_TRUE : VK_FALSE;
    const float priority = 1.0f;
    const VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queue_family_,
        .queueCount = 1,
        .pQueuePriorities = &priority,
    };
    const char* const device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    const VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = device_extensions,
    };
    check(vkCreateDevice(physical_device_, &device_info, nullptr, &device_), "create the Vulkan device");
    vkGetDeviceQueue(device_, queue_family_, 0, &queue_);
}

Gpu::~Gpu()
{
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        vkDestroyDevice(device_, nullptr);
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
    }
}

void Gpu::pick_device(wl_display* display)
{
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());

    int best = 5;
    for (VkPhysicalDevice device : devices) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);
        if (properties.apiVersion < VK_API_VERSION_1_3 || !has_extension(device, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
            continue;
        }
        const int64_t family = draw_and_present_family(device, display);
        if (family < 0 || rank(properties.deviceType) >= best) {
            continue;
        }
        best = rank(properties.deviceType);
        physical_device_ = device;
        queue_family_ = static_cast<uint32_t>(family);
        name_ = properties.deviceName;

        // The most samples that both colour and depth attachments have, up to 8.
        const VkSampleCountFlags counts =
            properties.limits.framebufferColorSampleCounts & properties.limits.framebufferDepthSampleCounts;
        samples_ = VK_SAMPLE_COUNT_1_BIT;
        for (VkSampleCountFlagBits c : {VK_SAMPLE_COUNT_8_BIT, VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_2_BIT}) {
            if ((counts & c) != 0) {
                samples_ = c;
                break;
            }
        }
    }
    if (physical_device_ == VK_NULL_HANDLE) {
        throw std::runtime_error("no Vulkan 1.3 device that can present to the compositor");
    }

    // A depth format that every device has for attachments: D32_SFLOAT or
    // D16_UNORM, the first one it has.
    for (VkFormat format : {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM}) {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(physical_device_, format, &properties);
        if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
            depth_format_ = format;
            break;
        }
    }
    if (depth_format_ == VK_FORMAT_UNDEFINED) {
        throw std::runtime_error("the Vulkan device " + name_ + " has no depth format");
    }
}

uint32_t Gpu::memory_type(uint32_t bits, VkMemoryPropertyFlags properties) const
{
    VkPhysicalDeviceMemoryProperties memory;
    vkGetPhysicalDeviceMemoryProperties(physical_device_, &memory);
    for (uint32_t i = 0; i < memory.memoryTypeCount; ++i) {
        if ((bits & (1u << i)) != 0 && (memory.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("the Vulkan device " + name_ + " has no fitting memory type");
}
