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
 * @brief The extensions of a device.
 * @param device The physical device.
 */
std::vector<VkExtensionProperties> extensions_of(VkPhysicalDevice device)
{
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> extensions(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data());
    return extensions;
}

/**
 * @brief Whether a list of extensions has one.
 * @param extensions The extensions of a device.
 * @param name       The name of the extension.
 */
bool has_extension(const std::vector<VkExtensionProperties>& extensions, const char* name)
{
    return std::any_of(extensions.begin(), extensions.end(),
                       [name](const VkExtensionProperties& e) { return std::strcmp(e.extensionName, name) == 0; });
}

/**
 * @brief The kind of a device.
 * @param type The type of the device, of Vulkan.
 */
DeviceKind kind_of(VkPhysicalDeviceType type)
{
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return DeviceKind::discrete;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return DeviceKind::integrated;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return DeviceKind::virtual_gpu;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        return DeviceKind::cpu;
    default:
        return DeviceKind::other;
    }
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

Gpu::Gpu(wl_display* display, std::optional<DrmNode> compositor, int msaa)
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

    const DeviceInfo info = pick_device(display, compositor, msaa);
    const bool core = core_features(info);

    VkPhysicalDeviceFeatures supported = {};
    vkGetPhysicalDeviceFeatures(physical_device_, &supported);
    sample_rate_shading_ = supported.sampleRateShading == VK_TRUE;

    // Dynamic rendering and synchronization2 have to be turned on: on Vulkan
    // 1.3 and later in the features of 1.3, on 1.2 in the structs of their
    // extensions, which are enabled too.
    VkPhysicalDeviceVulkan13Features features13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
    };
    VkPhysicalDeviceSynchronization2FeaturesKHR synchronization2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
        .synchronization2 = VK_TRUE,
    };
    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
        .pNext = &synchronization2,
        .dynamicRendering = VK_TRUE,
    };
    VkPhysicalDeviceFeatures2 features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = core ? static_cast<void*>(&features13) : static_cast<void*>(&dynamic_rendering),
    };
    features.features.sampleRateShading = sample_rate_shading_ ? VK_TRUE : VK_FALSE;
    const float priority = 1.0f;
    const VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queue_family_,
        .queueCount = 1,
        .pQueuePriorities = &priority,
    };
    std::vector<const char*> device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    if (!core) {
        device_extensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
        device_extensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    }
    const VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = static_cast<uint32_t>(device_extensions.size()),
        .ppEnabledExtensionNames = device_extensions.data(),
    };
    check(vkCreateDevice(physical_device_, &device_info, nullptr, &device_), "create the Vulkan device");
    vkGetDeviceQueue(device_, queue_family_, 0, &queue_);

    begin_rendering_ = reinterpret_cast<PFN_vkCmdBeginRendering>(command(core, "vkCmdBeginRendering"));
    end_rendering_ = reinterpret_cast<PFN_vkCmdEndRendering>(command(core, "vkCmdEndRendering"));
    pipeline_barrier2_ = reinterpret_cast<PFN_vkCmdPipelineBarrier2>(command(core, "vkCmdPipelineBarrier2"));
    queue_submit2_ = reinterpret_cast<PFN_vkQueueSubmit2>(command(core, "vkQueueSubmit2"));
}

PFN_vkVoidFunction Gpu::command(bool core, const std::string& name) const
{
    const std::string full = core ? name : name + "KHR";
    const PFN_vkVoidFunction function = vkGetDeviceProcAddr(device_, full.c_str());
    if (function == nullptr) {
        throw std::runtime_error("the Vulkan device " + name_ + " lacks " + full);
    }
    return function;
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

DeviceInfo Gpu::pick_device(wl_display* display, std::optional<DrmNode> compositor, int msaa)
{
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());

    // What pick_device() of gpu_pick.hpp needs to know of each device, with
    // its queue family that draws and presents.
    std::vector<DeviceInfo> infos;
    std::vector<int64_t> families;
    for (VkPhysicalDevice device : devices) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);
        const std::vector<VkExtensionProperties> extensions = extensions_of(device);
        const int64_t family = draw_and_present_family(device, display);
        DeviceInfo info = {
            .name = properties.deviceName,
            .version = {VK_API_VERSION_MAJOR(properties.apiVersion), VK_API_VERSION_MINOR(properties.apiVersion)},
            .kind = kind_of(properties.deviceType),
            .presents = family >= 0,
            .swapchain = has_extension(extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME),
            .dynamic_rendering = has_extension(extensions, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME),
            .synchronization2 = has_extension(extensions, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME),
        };
        // The DRM nodes come with vkGetPhysicalDeviceProperties2, of Vulkan 1.1.
        if (has_extension(extensions, VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME) &&
            properties.apiVersion >= VK_API_VERSION_1_1) {
            VkPhysicalDeviceDrmPropertiesEXT drm = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT,
            };
            VkPhysicalDeviceProperties2 properties2 = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                .pNext = &drm,
            };
            vkGetPhysicalDeviceProperties2(device, &properties2);
            if (drm.hasPrimary) {
                info.primary = DrmNode{drm.primaryMajor, drm.primaryMinor};
            }
            if (drm.hasRender) {
                info.render = DrmNode{drm.renderMajor, drm.renderMinor};
            }
        }
        infos.push_back(std::move(info));
        families.push_back(family);
    }

    const std::optional<std::size_t> picked = ::pick_device(infos, compositor);
    if (!picked.has_value()) {
        throw std::runtime_error("no Vulkan device that can present to the compositor, with Vulkan 1.3, "
                                 "or Vulkan 1.2 with VK_KHR_dynamic_rendering and VK_KHR_synchronization2");
    }
    physical_device_ = devices[*picked];
    queue_family_ = static_cast<uint32_t>(families[*picked]);
    name_ = infos[*picked].name;

    // The most samples that both colour and depth attachments have, up to msaa.
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical_device_, &properties);
    const VkSampleCountFlags counts =
        properties.limits.framebufferColorSampleCounts & properties.limits.framebufferDepthSampleCounts;
    samples_ = VK_SAMPLE_COUNT_1_BIT;
    for (VkSampleCountFlagBits c : {VK_SAMPLE_COUNT_8_BIT, VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_2_BIT}) {
        if ((counts & c) != 0 && static_cast<int>(c) <= msaa) {
            samples_ = c;
            break;
        }
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
    return infos[*picked];
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
