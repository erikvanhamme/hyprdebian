// The scenes of hd-logo; see logo.hpp.

#include "logo.hpp"

#include "bounce.hpp"
#include "buffer.hpp"
#include "gpu.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <optional>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

#include "logo.frag.h"
#include "logo.vert.h"
#include "logo_mask.h"

namespace {

/** @brief The ticks per second with framerate 0 while the refresh rate of the output is not known. */
constexpr double FALLBACK_RATE = 60.0;

/** @brief The push constants of logo.vert and logo.frag. */
struct Push {
    /** @brief The quad: left, top, right and bottom, from -1 to 1. */
    float rect[4];
    /** @brief The colour, with an alpha that is not used. */
    float colour[4];
};
static_assert(sizeof(Push) == 32);

/**
 * @brief Makes a shader module.
 * @param code The SPIR-V, as glslangValidator puts it in the header of the shader.
 */
template <std::size_t N>
VkShaderModule make_shader(VkDevice device, const uint32_t (&code)[N])
{
    const VkShaderModuleCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof code,
        .pCode = code,
    };
    VkShaderModule module = VK_NULL_HANDLE;
    check(vkCreateShaderModule(device, &info, nullptr, &module), "create a shader module");
    return module;
}

/**
 * @brief Records a layout transition of every level of the mask.
 * @param gpu   The device, which has the command of the barrier.
 * @param cmd   The command buffer.
 * @param image The mask.
 * @param from  The layout it has.
 * @param to    The layout it gets.
 * @param src_stage  The stages that must be done with it first.
 * @param src_access What those stages wrote.
 * @param dst_stage  The stages that wait for the transition.
 * @param dst_access What those stages do with it.
 */
void transition(const Gpu& gpu, VkCommandBuffer cmd, VkImage image, VkImageLayout from, VkImageLayout to,
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
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = LOGO_MASK_LEVELS,
                             .baseArrayLayer = 0,
                             .layerCount = 1},
    };
    const VkDependencyInfo dependency = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };
    gpu.cmd_pipeline_barrier2(cmd, &dependency);
}

} // namespace

/** @brief What the scenes share on the device: the mask, its sampler and descriptor set, and a pipeline per format. */
class LogoRenderer {
public:
    /**
     * @brief Puts the mask on the device, with all its levels, and makes the descriptor set and the layout.
     * @throws std::runtime_error When a Vulkan call fails.
     */
    explicit LogoRenderer(Gpu& gpu) : gpu_(gpu)
    {
        try {
            make_mask();
            make_layout();
        } catch (...) {
            destroy();
            throw;
        }
    }

    ~LogoRenderer() { destroy(); }

    LogoRenderer(const LogoRenderer&) = delete;
    LogoRenderer& operator=(const LogoRenderer&) = delete;

    /** @brief The layout of the pipelines, for the push constants and the descriptor set. */
    VkPipelineLayout layout() const { return layout_; }

    /** @brief The descriptor set of the mask. */
    VkDescriptorSet set() const { return set_; }

    /**
     * @brief The pipeline that draws into images of a format; made the first time.
     * @throws std::runtime_error When a Vulkan call fails.
     */
    VkPipeline pipeline(VkFormat format)
    {
        for (const auto& [f, pipeline] : pipelines_) {
            if (f == format) {
                return pipeline;
            }
        }
        const VkDevice device = gpu_.device();
        const VkShaderModule vert = make_shader(device, logo_vert);
        const VkShaderModule frag = make_shader(device, logo_frag);
        const VkPipelineShaderStageCreateInfo stages[] = {
            {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
             .stage = VK_SHADER_STAGE_VERTEX_BIT,
             .module = vert,
             .pName = "main"},
            {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
             .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
             .module = frag,
             .pName = "main"},
        };
        // No vertices: logo.vert makes the corners of the quad.
        const VkPipelineVertexInputStateCreateInfo input = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        };
        const VkPipelineInputAssemblyStateCreateInfo assembly = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        };
        const VkPipelineViewportStateCreateInfo viewport = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };
        const VkPipelineRasterizationStateCreateInfo raster = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .lineWidth = 1.0f,
        };
        // hd-logo asks for one sample per pixel: the quad lies on whole pixels.
        const VkPipelineMultisampleStateCreateInfo multisample = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = gpu_.samples(),
        };
        // The frame has a depth attachment, which the logo does not use.
        const VkPipelineDepthStencilStateCreateInfo depth = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_FALSE,
            .depthWriteEnable = VK_FALSE,
        };
        const VkPipelineColorBlendAttachmentState blend_attachment = {
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                              VK_COLOR_COMPONENT_A_BIT,
        };
        const VkPipelineColorBlendStateCreateInfo blend = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &blend_attachment,
        };
        const VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        const VkPipelineDynamicStateCreateInfo dynamic = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = 2,
            .pDynamicStates = dynamic_states,
        };
        const VkPipelineRenderingCreateInfo rendering = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &format,
            .depthAttachmentFormat = gpu_.depth_format(),
        };
        const VkGraphicsPipelineCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = &rendering,
            .stageCount = 2,
            .pStages = stages,
            .pVertexInputState = &input,
            .pInputAssemblyState = &assembly,
            .pViewportState = &viewport,
            .pRasterizationState = &raster,
            .pMultisampleState = &multisample,
            .pDepthStencilState = &depth,
            .pColorBlendState = &blend,
            .pDynamicState = &dynamic,
            .layout = layout_,
        };
        VkPipeline pipeline = VK_NULL_HANDLE;
        const VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);
        vkDestroyShaderModule(device, vert, nullptr);
        vkDestroyShaderModule(device, frag, nullptr);
        check(result, "create the pipeline");
        pipelines_.emplace_back(format, pipeline);
        return pipeline;
    }

private:
    /**
     * @brief Makes the image of the mask, copies every level of logo_mask into it and makes its view and sampler.
     *
     * The copy goes through a buffer, with a command buffer of its own, and
     * waits until it is done: it happens once, before the first frame.
     */
    void make_mask()
    {
        const VkDevice device = gpu_.device();
        const VkImageCreateInfo image_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_R8_UNORM,
            .extent = {LOGO_MASK_WIDTH, LOGO_MASK_HEIGHT, 1},
            .mipLevels = LOGO_MASK_LEVELS,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };
        check(vkCreateImage(device, &image_info, nullptr, &image_), "create the image of the mask");
        VkMemoryRequirements requirements;
        vkGetImageMemoryRequirements(device, image_, &requirements);
        const VkMemoryAllocateInfo alloc = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = requirements.size,
            .memoryTypeIndex = gpu_.memory_type(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        };
        check(vkAllocateMemory(device, &alloc, nullptr, &memory_), "allocate the memory of the mask");
        check(vkBindImageMemory(device, image_, memory_, 0), "bind the memory of the mask");

        Buffer staging(gpu_, sizeof logo_mask, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        std::memcpy(staging.data(), logo_mask, sizeof logo_mask);

        const VkCommandPoolCreateInfo pool_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = gpu_.queue_family(),
        };
        VkCommandPool pool = VK_NULL_HANDLE;
        check(vkCreateCommandPool(device, &pool_info, nullptr, &pool), "create a command pool");
        struct PoolGuard {
            VkDevice device;
            VkCommandPool pool;
            ~PoolGuard() { vkDestroyCommandPool(device, pool, nullptr); }
        } guard{device, pool};

        const VkCommandBufferAllocateInfo cmd_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        check(vkAllocateCommandBuffers(device, &cmd_info, &cmd), "allocate a command buffer");
        const VkCommandBufferBeginInfo begin = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        check(vkBeginCommandBuffer(cmd, &begin), "begin a command buffer");
        transition(gpu_, cmd, image_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COPY_BIT,
                   VK_ACCESS_2_TRANSFER_WRITE_BIT);
        std::vector<VkBufferImageCopy> regions;
        for (uint32_t level = 0; level < LOGO_MASK_LEVELS; level++) {
            regions.push_back({
                .bufferOffset = LOGO_MASK_OFFSETS[level],
                .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                     .mipLevel = level,
                                     .baseArrayLayer = 0,
                                     .layerCount = 1},
                .imageExtent = {std::max(1u, LOGO_MASK_WIDTH >> level), std::max(1u, LOGO_MASK_HEIGHT >> level), 1},
            });
        }
        vkCmdCopyBufferToImage(cmd, staging.buffer(), image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               static_cast<uint32_t>(regions.size()), regions.data());
        transition(gpu_, cmd, image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                   VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                   VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
        check(vkEndCommandBuffer(cmd), "record the copy of the mask");
        const VkCommandBufferSubmitInfo cmd_submit = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = cmd,
        };
        const VkSubmitInfo2 submit = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &cmd_submit,
        };
        check(gpu_.queue_submit2(1, &submit, VK_NULL_HANDLE), "submit the copy of the mask");
        check(vkQueueWaitIdle(gpu_.queue()), "wait for the copy of the mask");

        const VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image_,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_R8_UNORM,
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                 .baseMipLevel = 0,
                                 .levelCount = LOGO_MASK_LEVELS,
                                 .baseArrayLayer = 0,
                                 .layerCount = 1},
        };
        check(vkCreateImageView(device, &view_info, nullptr, &view_), "create the view of the mask");
        // Linear within and between the levels; the edges of the mask are empty.
        const VkSamplerCreateInfo sampler_info = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .maxLod = static_cast<float>(LOGO_MASK_LEVELS),
        };
        check(vkCreateSampler(device, &sampler_info, nullptr, &sampler_), "create the sampler of the mask");
    }

    /**
     * @brief Makes the descriptor set of the mask and the layout of the pipelines.
     */
    void make_layout()
    {
        const VkDevice device = gpu_.device();
        const VkDescriptorSetLayoutBinding binding = {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        };
        const VkDescriptorSetLayoutCreateInfo set_layout_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings = &binding,
        };
        check(vkCreateDescriptorSetLayout(device, &set_layout_info, nullptr, &set_layout_),
              "create the descriptor set layout");
        const VkDescriptorPoolSize size = {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1};
        const VkDescriptorPoolCreateInfo pool_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &size,
        };
        check(vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptor_pool_), "create the descriptor pool");
        const VkDescriptorSetAllocateInfo set_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = descriptor_pool_,
            .descriptorSetCount = 1,
            .pSetLayouts = &set_layout_,
        };
        check(vkAllocateDescriptorSets(device, &set_info, &set_), "allocate the descriptor set");
        const VkDescriptorImageInfo image_info = {
            .sampler = sampler_,
            .imageView = view_,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        const VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = set_,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &image_info,
        };
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

        const VkPushConstantRange push = {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(Push),
        };
        const VkPipelineLayoutCreateInfo layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &set_layout_,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &push,
        };
        check(vkCreatePipelineLayout(device, &layout_info, nullptr, &layout_), "create the pipeline layout");
    }

    /** @brief Destroys what is made, after the device is idle. */
    void destroy()
    {
        const VkDevice device = gpu_.device();
        vkDeviceWaitIdle(device);
        for (const auto& [format, pipeline] : pipelines_) {
            vkDestroyPipeline(device, pipeline, nullptr);
        }
        pipelines_.clear();
        // Destroying null handles does nothing.
        vkDestroyPipelineLayout(device, layout_, nullptr);
        vkDestroyDescriptorPool(device, descriptor_pool_, nullptr);
        vkDestroyDescriptorSetLayout(device, set_layout_, nullptr);
        vkDestroySampler(device, sampler_, nullptr);
        vkDestroyImageView(device, view_, nullptr);
        vkDestroyImage(device, image_, nullptr);
        vkFreeMemory(device, memory_, nullptr);
        layout_ = VK_NULL_HANDLE;
        descriptor_pool_ = VK_NULL_HANDLE;
        set_layout_ = VK_NULL_HANDLE;
        sampler_ = VK_NULL_HANDLE;
        view_ = VK_NULL_HANDLE;
        image_ = VK_NULL_HANDLE;
        memory_ = VK_NULL_HANDLE;
    }

    /** @brief The device. */
    Gpu& gpu_;
    /** @brief The mask, with all its levels, its memory, view and sampler. */
    VkImage image_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    VkImageView view_ = VK_NULL_HANDLE;
    VkSampler sampler_ = VK_NULL_HANDLE;
    /** @brief The descriptor set of the mask, its layout and pool. */
    VkDescriptorSetLayout set_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
    VkDescriptorSet set_ = VK_NULL_HANDLE;
    /** @brief The layout of the pipelines. */
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    /** @brief The pipelines, with the format of the images they draw into. */
    std::vector<std::pair<VkFormat, VkPipeline>> pipelines_;
};

namespace {

/** @brief The logo on one surface. */
class LogoScene : public Scene {
public:
    /**
     * @param renderer  The mask and the pipeline layout; it must outlive the scene.
     * @param pipeline  The pipeline for the format of the surface.
     * @param framerate The ticks per second; 0 for the refresh rate of the output.
     * @param palette   The colours.
     * @param seed      The seed of the start.
     */
    LogoScene(const LogoRenderer& renderer, VkPipeline pipeline, int framerate, const LogoPalette& palette,
              uint32_t seed)
        : renderer_(renderer), pipeline_(pipeline), framerate_(framerate), palette_(palette), random_(seed)
    {
    }

    void draw(VkCommandBuffer cmd, VkExtent2D extent, int refresh) override
    {
        const auto now = std::chrono::steady_clock::now();
        double rate = framerate_;
        if (rate == 0) {
            rate = refresh > 0 ? refresh / 1000.0 : FALLBACK_RATE;
        }
        if (!bounce_ || extent.width != extent_.width || extent.height != extent_.height || rate != rate_) {
            // A new size or rate: a new layout and a new start, at tick 0.
            extent_ = extent;
            rate_ = rate;
            const Layout layout = find_layout(static_cast<int>(extent.width), static_cast<int>(extent.height),
                                              static_cast<double>(LOGO_MASK_WIDTH) / LOGO_MASK_HEIGHT, rate);
            bounce_.emplace(layout, random_);
            start_ = now;
            ticks_ = 0;
        }
        const int64_t tick = tick_at(now - start_, rate_);
        for (; ticks_ < tick; ticks_++) {
            bounce_->tick();
        }

        const Layout& layout = bounce_->layout();
        const float width = static_cast<float>(extent.width);
        const float height = static_cast<float>(extent.height);
        const Rgb& colour = palette_[bounce_->colour()];
        const Push push = {
            .rect = {2.0f * bounce_->x() / width - 1.0f, 2.0f * bounce_->y() / height - 1.0f,
                     2.0f * (bounce_->x() + layout.width) / width - 1.0f,
                     2.0f * (bounce_->y() + layout.height) / height - 1.0f},
            .colour = {colour.r, colour.g, colour.b, 1.0f},
        };
        const VkDescriptorSet set = renderer_.set();
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer_.layout(), 0, 1, &set, 0, nullptr);
        vkCmdPushConstants(cmd, renderer_.layout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof push, &push);
        vkCmdDraw(cmd, 4, 1, 0, 0);
    }

private:
    /** @brief The mask and the pipeline layout. */
    const LogoRenderer& renderer_;
    /** @brief The pipeline. */
    VkPipeline pipeline_;
    /** @brief The ticks per second; 0 for the refresh rate. */
    int framerate_;
    /** @brief The colours. */
    LogoPalette palette_;
    /** @brief The random numbers of the starts. */
    std::mt19937 random_;
    /** @brief The logo; none before the first frame. */
    std::optional<Bounce> bounce_;
    /** @brief The size of the surface and the ticks per second that the layout is for. */
    VkExtent2D extent_ = {0, 0};
    double rate_ = 0;
    /** @brief When tick 0 was. */
    std::chrono::steady_clock::time_point start_;
    /** @brief The ticks done since the start. */
    int64_t ticks_ = 0;
};

} // namespace

Logo::Logo(const LogoOptions& options, const LogoPalette& palette) : options_(options), palette_(palette) {}

Logo::~Logo() = default;

std::unique_ptr<Scene> Logo::make_scene(Gpu& gpu, VkFormat format)
{
    if (renderer_ == nullptr) {
        renderer_ = std::make_unique<LogoRenderer>(gpu);
    }
    // A start of its own for every monitor.
    std::random_device device;
    return std::make_unique<LogoScene>(*renderer_, renderer_->pipeline(format), options_.framerate, palette_,
                                       device());
}
