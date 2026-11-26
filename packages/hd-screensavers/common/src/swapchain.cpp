#include "swapchain.hpp"

#include "gpu.hpp"

#include <algorithm>
#include <stdexcept>

namespace {

/** @brief The frames in flight: the CPU records one while the GPU draws the other. */
constexpr std::size_t FRAMES = 2;

/**
 * @brief Records a layout transition of an image.
 * @param gpu         The device, which has the command of the barrier.
 * @param cmd         The command buffer.
 * @param image       The image.
 * @param aspect      The aspect of the image.
 * @param from        The layout it has.
 * @param to          The layout it gets.
 * @param src_stage   The stages that must be done with it first.
 * @param src_access  What those stages wrote.
 * @param dst_stage   The stages that wait for the transition.
 * @param dst_access  What those stages do with it.
 */
void transition(const Gpu& gpu, VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspect, VkImageLayout from, VkImageLayout to,
                VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access, VkPipelineStageFlags2 dst_stage,
                VkAccessFlags2 dst_access)
{
    const VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = src_stage,
        .srcAccessMask = src_access,
        .dstStageMask = dst_stage,
        .dstAccessMask = dst_access,
        .oldLayout = from,
        .newLayout = to,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {.aspectMask = aspect, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1},
    };
    const VkDependencyInfo dependency = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };
    gpu.cmd_pipeline_barrier2(cmd, &dependency);
}

/**
 * @brief Makes a view of an image.
 * @param device The device.
 * @param image  The image.
 * @param format The format of the image.
 * @param aspect The aspect of the view.
 * @throws std::runtime_error When the view cannot be made.
 */
VkImageView make_view(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect)
{
    const VkImageViewCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = {.aspectMask = aspect, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1},
    };
    VkImageView view = VK_NULL_HANDLE;
    check(vkCreateImageView(device, &info, nullptr, &view), "create an image view");
    return view;
}

} // namespace

Swapchain::Swapchain(Gpu& gpu, wl_display* display, wl_surface* surface) : gpu_(gpu)
{
    const VkWaylandSurfaceCreateInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
        .display = display,
        .surface = surface,
    };
    check(vkCreateWaylandSurfaceKHR(gpu_.instance(), &info, nullptr, &surface_), "create the Vulkan surface");
    VkBool32 supported = VK_FALSE;
    check(vkGetPhysicalDeviceSurfaceSupportKHR(gpu_.physical_device(), gpu_.queue_family(), surface_, &supported),
          "query the surface support");
    if (!supported) {
        throw std::runtime_error("the Vulkan device " + gpu_.name() + " cannot present to the surface");
    }

    // 8 bit UNORM, in the order the surface has it. Not sRGB: the colours of
    // the shaders go to the screen as they are.
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu_.physical_device(), surface_, &count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu_.physical_device(), surface_, &count, formats.data());
    for (VkFormat wanted : {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM}) {
        for (const VkSurfaceFormatKHR& f : formats) {
            if (format_ == VK_FORMAT_UNDEFINED && f.format == wanted &&
                f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                format_ = f.format;
                color_space_ = f.colorSpace;
            }
        }
    }
    if (format_ == VK_FORMAT_UNDEFINED) {
        throw std::runtime_error("the surface has no 8 bit UNORM format");
    }

    const VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = gpu_.queue_family(),
    };
    check(vkCreateCommandPool(gpu_.device(), &pool_info, nullptr, &pool_), "create a command pool");
    frames_.resize(FRAMES);
    for (Frame& f : frames_) {
        const VkCommandBufferAllocateInfo cmd_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = pool_,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        check(vkAllocateCommandBuffers(gpu_.device(), &cmd_info, &f.cmd), "allocate a command buffer");
        const VkSemaphoreCreateInfo semaphore_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        check(vkCreateSemaphore(gpu_.device(), &semaphore_info, nullptr, &f.acquired), "create a semaphore");
        // Signalled, so that the first wait of draw() returns at once.
        const VkFenceCreateInfo fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };
        check(vkCreateFence(gpu_.device(), &fence_info, nullptr, &f.done), "create a fence");
    }
}

Swapchain::~Swapchain()
{
    const VkDevice device = gpu_.device();
    vkDeviceWaitIdle(device);
    destroy_images();
    for (Frame& f : frames_) {
        if (f.done != VK_NULL_HANDLE) {
            vkDestroyFence(device, f.done, nullptr);
        }
        if (f.acquired != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, f.acquired, nullptr);
        }
    }
    if (pool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, pool_, nullptr);
    }
    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapchain_, nullptr);
    }
    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(gpu_.instance(), surface_, nullptr);
    }
}

void Swapchain::resize(uint32_t width, uint32_t height)
{
    if (width != extent_.width || height != extent_.height) {
        extent_ = {width, height};
        stale_ = true;
    }
}

bool Swapchain::draw(const Draw& draw)
{
    if (extent_.width == 0 || extent_.height == 0) {
        return false;
    }
    const VkDevice device = gpu_.device();
    Frame& f = frames_[frame_];
    check(vkWaitForFences(device, 1, &f.done, VK_TRUE, UINT64_MAX), "wait for a frame");
    if (stale_) {
        create();
    }

    uint32_t image = 0;
    VkResult result = vkAcquireNextImageKHR(device, swapchain_, UINT64_MAX, f.acquired, VK_NULL_HANDLE, &image);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        stale_ = true;
        return false;
    }
    check(result, "acquire a swapchain image");
    // A suboptimal image still gets this frame; the next one gets a new swapchain.
    if (result == VK_SUBOPTIMAL_KHR) {
        stale_ = true;
    }

    check(vkResetFences(device, 1, &f.done), "reset a fence");
    record(image, draw);

    const VkSemaphoreSubmitInfo wait = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = f.acquired,
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    };
    const VkSemaphoreSubmitInfo signal = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = rendered_[image],
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    };
    const VkCommandBufferSubmitInfo cmd = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = f.cmd,
    };
    const VkSubmitInfo2 submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = &wait,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmd,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signal,
    };
    check(gpu_.queue_submit2(1, &submit, f.done), "submit a frame");
    frame_ = (frame_ + 1) % frames_.size();

    const VkPresentInfoKHR present = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &rendered_[image],
        .swapchainCount = 1,
        .pSwapchains = &swapchain_,
        .pImageIndices = &image,
    };
    result = vkQueuePresentKHR(gpu_.queue(), &present);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        stale_ = true;
        return result == VK_SUBOPTIMAL_KHR;
    }
    check(result, "present a frame");
    return true;
}

void Swapchain::create()
{
    const VkDevice device = gpu_.device();
    // The old images may still be in use by the frames in flight.
    vkDeviceWaitIdle(device);
    destroy_images();

    VkSurfaceCapabilitiesKHR caps;
    check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu_.physical_device(), surface_, &caps),
          "query the surface capabilities");
    // On Wayland the surface takes the size of its images, and currentExtent
    // is 0xFFFFFFFF; the size of the configure of the surface goes.
    if (caps.currentExtent.width != UINT32_MAX) {
        extent_ = caps.currentExtent;
    }
    extent_.width = std::clamp(extent_.width, caps.minImageExtent.width, caps.maxImageExtent.width);
    extent_.height = std::clamp(extent_.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    // Three images or more: one on the screen, one waiting for the vsync and
    // one to draw into.
    uint32_t count = std::max(caps.minImageCount + 1, 3u);
    if (caps.maxImageCount != 0) {
        count = std::min(count, caps.maxImageCount);
    }
    VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if ((caps.supportedCompositeAlpha & alpha) == 0) {
        alpha = static_cast<VkCompositeAlphaFlagBitsKHR>(caps.supportedCompositeAlpha &
                                                         -caps.supportedCompositeAlpha);
    }

    const VkSwapchainKHR old = swapchain_;
    const VkSwapchainCreateInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface_,
        .minImageCount = count,
        .imageFormat = format_,
        .imageColorSpace = color_space_,
        .imageExtent = extent_,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = alpha,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
        .oldSwapchain = old,
    };
    swapchain_ = VK_NULL_HANDLE;
    const VkResult result = vkCreateSwapchainKHR(device, &info, nullptr, &swapchain_);
    if (old != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, old, nullptr);
    }
    check(result, "create the swapchain");

    vkGetSwapchainImagesKHR(device, swapchain_, &count, nullptr);
    images_.resize(count);
    vkGetSwapchainImagesKHR(device, swapchain_, &count, images_.data());
    for (VkImage image : images_) {
        views_.push_back(make_view(device, image, format_, VK_IMAGE_ASPECT_COLOR_BIT));
        const VkSemaphoreCreateInfo semaphore_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkSemaphore semaphore = VK_NULL_HANDLE;
        check(vkCreateSemaphore(device, &semaphore_info, nullptr, &semaphore), "create a semaphore");
        rendered_.push_back(semaphore);
    }
    // Both attachments live only within a frame: transient, never stored.
    if (gpu_.samples() != VK_SAMPLE_COUNT_1_BIT) {
        color_ = make_attachment(format_, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
                                 VK_IMAGE_ASPECT_COLOR_BIT, gpu_.samples());
    }
    depth_ = make_attachment(gpu_.depth_format(),
                             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
                             VK_IMAGE_ASPECT_DEPTH_BIT, gpu_.samples());
    stale_ = false;
}

void Swapchain::destroy_images()
{
    const VkDevice device = gpu_.device();
    destroy_attachment(color_);
    destroy_attachment(depth_);
    for (VkImageView view : views_) {
        vkDestroyImageView(device, view, nullptr);
    }
    for (VkSemaphore semaphore : rendered_) {
        vkDestroySemaphore(device, semaphore, nullptr);
    }
    views_.clear();
    rendered_.clear();
    images_.clear();
}

Swapchain::Attachment Swapchain::make_attachment(VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect,
                                                 VkSampleCountFlagBits samples)
{
    const VkDevice device = gpu_.device();
    Attachment a;
    const VkImageCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {extent_.width, extent_.height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = samples,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    check(vkCreateImage(device, &info, nullptr, &a.image), "create an attachment");
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(device, a.image, &requirements);
    const VkMemoryAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = gpu_.memory_type(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };
    check(vkAllocateMemory(device, &alloc, nullptr, &a.memory), "allocate the memory of an attachment");
    check(vkBindImageMemory(device, a.image, a.memory, 0), "bind the memory of an attachment");
    a.view = make_view(device, a.image, format, aspect);
    return a;
}

void Swapchain::destroy_attachment(Attachment& a)
{
    const VkDevice device = gpu_.device();
    if (a.view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, a.view, nullptr);
    }
    if (a.image != VK_NULL_HANDLE) {
        vkDestroyImage(device, a.image, nullptr);
    }
    if (a.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, a.memory, nullptr);
    }
    a = Attachment();
}

void Swapchain::record(uint32_t image, const Draw& draw)
{
    const VkCommandBuffer cmd = frames_[frame_].cmd;
    check(vkResetCommandBuffer(cmd, 0), "reset a command buffer");
    const VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    check(vkBeginCommandBuffer(cmd, &begin), "begin a command buffer");

    // Every frame clears its attachments, so what they held does not matter:
    // from UNDEFINED. The attachments are shared by the frames in flight; the
    // barriers wait for the writes of the frame before.
    const bool multisampled = gpu_.samples() != VK_SAMPLE_COUNT_1_BIT;
    constexpr VkPipelineStageFlags2 color_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    constexpr VkPipelineStageFlags2 depth_stage =
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    transition(gpu_, cmd, images_[image], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, color_stage, 0, color_stage,
               VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    if (multisampled) {
        transition(gpu_, cmd, color_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, color_stage, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                   color_stage, VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    }
    // The depth goes in DEPTH_STENCIL_ATTACHMENT_OPTIMAL of Vulkan 1.0, which
    // depth formats without stencil have too; DEPTH_ATTACHMENT_OPTIMAL would
    // need the feature separateDepthStencilLayouts turned on.
    transition(gpu_, cmd, depth_.image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
               VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, depth_stage, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
               depth_stage,
               VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

    const VkRenderingAttachmentInfo color = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = multisampled ? color_.view : views_[image],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .resolveMode = multisampled ? VK_RESOLVE_MODE_AVERAGE_BIT : VK_RESOLVE_MODE_NONE,
        .resolveImageView = multisampled ? views_[image] : VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = multisampled ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {.color = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f}}},
    };
    const VkRenderingAttachmentInfo depth = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = depth_.view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = {.depthStencil = {.depth = 1.0f, .stencil = 0}},
    };
    const VkRenderingInfo rendering = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {{0, 0}, extent_},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color,
        .pDepthAttachment = &depth,
    };
    gpu_.cmd_begin_rendering(cmd, &rendering);
    // The pipelines of the scene take the viewport and scissor as dynamic state.
    const VkViewport viewport = {0.0f, 0.0f, static_cast<float>(extent_.width), static_cast<float>(extent_.height),
                                 0.0f, 1.0f};
    const VkRect2D scissor = {{0, 0}, extent_};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    draw(cmd, extent_);
    gpu_.cmd_end_rendering(cmd);

    transition(gpu_, cmd, images_[image], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, color_stage, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
               VK_PIPELINE_STAGE_2_NONE, 0);
    check(vkEndCommandBuffer(cmd), "end a command buffer");
}
