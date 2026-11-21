/*
 * Based on hacks/glx/pipes.c of xscreensaver, from xlockmore, which carries
 * this notice:
 *
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 *
 *   This file is provided AS IS with no warranties of any kind.  The author
 *   shall have no liability with respect to the infringement of copyrights,
 *   trade secrets or any patents by this file or any part thereof.  In no
 *   event will the author be liable for any lost revenue or profits or
 *   other special, indirect and consequential damages.
 *
 *   This program was inspired on a WindowsNT(R)'s screen saver. It was written
 *   from scratch and it was not based on any other source code.
 *
 *   ==========================================================================
 *   The routine myElbow is derivated from the doughnut routine from the MesaGL
 *   library (more especifically the Mesaaux library) written by Brian Paul.
 *   ==========================================================================
 *
 *   Thanks goes to Brian Paul for making it possible and inexpensive to use
 *   OpenGL at home.
 *
 *   Since I'm not a native English speaker, my apologies for any grammatical
 *   mistake.
 *
 *   My e-mail address is
 *   m-vianna@usa.net
 *   Marcelo F. Vianna (Apr-09-1997)
 */

// The scenes of hd-pipes, after pinit, generate_system and draw_pipes of
// pipes.c; see pipes.hpp.

#include "pipes.hpp"

#include "buffer.hpp"
#include "gpu.hpp"
#include "matrix.hpp"
#include "mesh.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <random>
#include <utility>
#include <vector>

#include "pipe.frag.h"
#include "pipe.vert.h"

namespace {

/** @brief Scale4Window of pipes.c: the grid, 40 units wide, becomes 4 wide. */
constexpr float SCENE_SCALE = 0.1f;
/** @brief How far the camera is from the middle of the scene, with and without fisheye. */
constexpr float FISHEYE_DISTANCE = 3.8f;
constexpr float DISTANCE = 4.8f;
/** @brief The field of view from bottom to top, and the near and far planes, of gluPerspective in pipes.c. */
constexpr float FOVY = 65.0f;
constexpr float NEAR = 0.1f;
constexpr float FAR = 20.0f;
/** @brief The turn about the Y axis per step, in degrees. */
constexpr float ROTATION_STEP = 0.02f;
/** @brief The restart: from 100 down by 4 per step, fadeout of pipes.c. */
constexpr int FADE_START = 100;
constexpr int FADE_STEP = 4;
/**
 * @brief The most steps a frame catches up on.
 *
 * After a long frame, or a surface that was not drawn for a while, the
 * scene goes on from where it was instead of jumping ahead.
 */
constexpr int64_t MAX_STEPS_PER_FRAME = 25;

/** @brief The meshes, in the order of the regions of the instance buffers. */
enum MeshKind : std::size_t {
    TUBE,
    ELBOW,
    BALL,
    FLANGE,
    BOLT,
    BODY,
    END_PLATES,
    WHEEL,
    CONNECTOR,
    HEAD,
    FACE,
    DIAL,
    TEAPOT,
    MESHES
};

/** @brief The colour of the bolts, MaterialGray of pipes.c, after the colours of the systems in pipe.frag. */
constexpr int GREY = COLOURS;

/**
 * @brief The most instances of a step that moves on: a gauge, of six meshes, and the link.
 *
 * A bolted elbow and its link are four; the start and the cap come once per pipe.
 */
constexpr VkDeviceSize MOST_INSTANCES_PER_STEP = 7;

/** @brief A piece of a pipe as the vertex shader takes it: a mesh, turned, scaled and moved. */
struct Instance {
    /** @brief Where the x, y and z axes of the mesh go, with the scale. */
    float axes[3][3];
    /** @brief Where the origin of the mesh goes, in the units of pipes.c. */
    float position[3];
    /** @brief The colour of the system, an index into the colours of pipe.frag. */
    uint32_t colour;
};
static_assert(sizeof(Instance) == 52);

/** @brief The push constants of pipe.vert. */
struct Transform {
    Mat4 modelview;
    Mat4 projection;
};
static_assert(sizeof(Transform) == 128, "the push constants every device has");

using Vec3 = std::array<float, 3>;

Vec3 vector(Direction d)
{
    switch (d) {
    case Direction::up:
        return {0, 1, 0};
    case Direction::down:
        return {0, -1, 0};
    case Direction::left:
        return {-1, 0, 0};
    case Direction::right:
        return {1, 0, 0};
    case Direction::near:
        return {0, 0, 1};
    case Direction::far:
        return {0, 0, -1};
    }
    return {0, 0, 0};
}

Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

/** @brief Where the middle of a cell is, in the units of pipes.c, with the middle of the grid at the origin. */
Vec3 centre(Cell c)
{
    return {(c.x - GRID_WIDTH / 2) * CELL_SIZE, (c.y - GRID_HEIGHT / 2) * CELL_SIZE,
            (c.z - GRID_DEPTH / 2) * CELL_SIZE};
}

/** @brief An instance with the axes x, y and z of the mesh at @p x, @p y and @p z, times @p scale. */
Instance place(const Vec3& x, const Vec3& y, const Vec3& z, float scale, const Vec3& position, int colour)
{
    Instance i;
    for (int k = 0; k < 3; k++) {
        i.axes[0][k] = x[k] * scale;
        i.axes[1][k] = y[k] * scale;
        i.axes[2][k] = z[k] * scale;
        i.position[k] = position[k];
    }
    i.colour = static_cast<uint32_t>(colour);
    return i;
}

/** @brief An instance with the axes and the origin of the mesh where @p m takes them. */
Instance place(const Mat4& m, int colour)
{
    Instance i;
    for (int k = 0; k < 3; k++) {
        for (int c = 0; c < 3; c++) {
            i.axes[c][k] = m.m[c * 4 + k];
        }
        i.position[k] = m.m[12 + k];
    }
    i.colour = static_cast<uint32_t>(colour);
    return i;
}

/**
 * @brief The turn of a valve or the teapot onto its pipe, MakeValve and MakeTeapot of pipes.c.
 *
 * Their z axis goes along the pipe; @p turn, of the walk, turns them about
 * it by quarter turns.
 */
Mat4 turn_onto(Direction d, int turn)
{
    switch (d) {
    case Direction::up:
    case Direction::down:
        return rotate(90.0f, 1.0f, 0.0f, 0.0f) * rotate(turn * 90.0f, 0.0f, 0.0f, 1.0f);
    case Direction::left:
    case Direction::right:
        return rotate(90.0f, 0.0f, -1.0f, 0.0f) * rotate(turn * 90.0f - 90.0f, 0.0f, 0.0f, 1.0f);
    case Direction::near:
    case Direction::far:
        break;
    }
    return rotate(turn * 90.0f, 0.0f, 0.0f, 1.0f);
}

/** @brief A tube along @p d about @p position; its own z axis goes along d. */
Instance tube(Direction d, const Vec3& position, int colour)
{
    const Vec3 z = vector(d);
    // Any axis across d; the rings of the tube look the same turned by 90°.
    const Vec3 x = z[1] != 0 ? Vec3{1, 0, 0} : Vec3{0, 1, 0};
    return place(x, cross(z, x), z, 1.0f, position, colour);
}

/** @brief A ball of radius @p radius about @p position. */
Instance ball(float radius, const Vec3& position, int colour)
{
    return place({1, 0, 0}, {0, 1, 0}, {0, 0, 1}, radius, position, colour);
}

/** @brief The instances of a screenful, per mesh. */
using Instances = std::array<std::vector<Instance>, MESHES>;

/** @brief Adds the instances of a piece: one, three for a bolted elbow and a valve, six for a gauge. */
void add_instances(const Piece& p, int colour, Instances& instances)
{
    const Vec3 middle = centre(p.cell);
    switch (p.kind) {
    case Piece::Kind::start:
    case Piece::Kind::cap:
        instances[BALL].push_back(ball(END_RADIUS, middle, colour));
        return;
    case Piece::Kind::ball:
        instances[BALL].push_back(ball(JOINT_RADIUS, middle, colour));
        return;
    case Piece::Kind::straight:
        instances[TUBE].push_back(tube(p.out, middle, colour));
        return;
    case Piece::Kind::link: {
        const Vec3 next = centre(p.cell.next(p.out));
        instances[TUBE].push_back(
            tube(p.out, {(middle[0] + next[0]) / 2, (middle[1] + next[1]) / 2, (middle[2] + next[2]) / 2}, colour));
        return;
    }
    case Piece::Kind::elbow:
    case Piece::Kind::bolted: {
        // The elbow of mesh.hpp comes in along its z and goes out along its x;
        // the flanges and the bolts are in the same place.
        const Vec3 z = vector(p.in);
        const Vec3 x = vector(p.out);
        const Vec3 y = cross(z, x);
        instances[ELBOW].push_back(place(x, y, z, 1.0f, middle, colour));
        if (p.kind == Piece::Kind::bolted) {
            instances[FLANGE].push_back(place(x, y, z, 1.0f, middle, colour));
            instances[BOLT].push_back(place(x, y, z, 1.0f, middle, GREY));
        }
        return;
    }
    case Piece::Kind::valve: {
        // MakeValve: the body in the colour of the system, the plates grey, the wheel turned
        // about its stem.
        const Mat4 valve = translate(middle[0], middle[1], middle[2]) * turn_onto(p.out, p.turn);
        instances[BODY].push_back(place(valve, colour));
        instances[END_PLATES].push_back(place(valve, GREY));
        instances[WHEEL].push_back(place(valve * rotate(static_cast<float>(p.angle), 1.0f, 0.0f, 0.0f), p.colour));
        return;
    }
    case Piece::Kind::gauge: {
        // MakeGuage: only the body turns onto a pipe along x; the gauge always stands up and
        // faces +z. The needle turns clockwise from straight down.
        const Mat4 cell = translate(middle[0], middle[1], middle[2]);
        Mat4 body = cell;
        if (p.out == Direction::left || p.out == Direction::right) {
            body = cell * rotate(90.0f, 0.0f, 1.0f, 0.0f);
        }
        instances[BODY].push_back(place(body, colour));
        instances[END_PLATES].push_back(place(body, GREY));
        instances[CONNECTOR].push_back(place(cell, GREY));
        instances[HEAD].push_back(place(cell, colour));
        instances[FACE].push_back(place(cell, WHITE));
        instances[DIAL].push_back(place(cell * translate(0.0f, GAUGE_HEIGHT, 0.0f) *
                                            rotate(static_cast<float>(p.angle), 0.0f, 0.0f, -1.0f),
                                        GREY));
        return;
    }
    case Piece::Kind::teapot:
        instances[TEAPOT].push_back(place(translate(middle[0], middle[1], middle[2]) * turn_onto(p.out, p.turn), colour));
        return;
    }
}

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

} // namespace

/** @brief What the scenes share on the device: the meshes, the pipeline layout and a pipeline per format. */
class Renderer {
public:
    /** @brief Where a mesh is in the vertex and index buffers. */
    struct Part {
        uint32_t first_index = 0;
        uint32_t index_count = 0;
        int32_t vertex_offset = 0;
    };

    /**
     * @brief Makes the meshes and puts them on the device.
     * @throws std::runtime_error When a Vulkan call fails.
     */
    explicit Renderer(Gpu& gpu) : gpu_(gpu)
    {
        const std::array<Mesh, MESHES> meshes = {
            make_tube(),  make_elbow(),      make_ball(),  make_flanges(), make_bolts(), make_body(),  make_end_plates(),
            make_wheel(), make_connector(), make_head(), make_face(),    make_dial(),  make_teapot(),
        };
        std::size_t vertex_count = 0;
        std::size_t index_count = 0;
        for (std::size_t m = 0; m < MESHES; m++) {
            parts_[m] = {static_cast<uint32_t>(index_count), static_cast<uint32_t>(meshes[m].indices.size()),
                         static_cast<int32_t>(vertex_count)};
            vertex_count += meshes[m].vertices.size();
            index_count += meshes[m].indices.size();
        }
        vertices_ = std::make_unique<Buffer>(gpu_, vertex_count * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        indices_ = std::make_unique<Buffer>(gpu_, index_count * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        auto* vertex = static_cast<Vertex*>(vertices_->data());
        auto* index = static_cast<uint32_t*>(indices_->data());
        for (const Mesh& mesh : meshes) {
            vertex = std::copy(mesh.vertices.begin(), mesh.vertices.end(), vertex);
            index = std::copy(mesh.indices.begin(), mesh.indices.end(), index);
        }

        const VkPushConstantRange push = {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(Transform),
        };
        const VkPipelineLayoutCreateInfo layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &push,
        };
        check(vkCreatePipelineLayout(gpu_.device(), &layout_info, nullptr, &layout_), "create the pipeline layout");
    }

    ~Renderer()
    {
        const VkDevice device = gpu_.device();
        vkDeviceWaitIdle(device);
        for (const auto& [format, pipeline] : pipelines_) {
            vkDestroyPipeline(device, pipeline, nullptr);
        }
        vkDestroyPipelineLayout(device, layout_, nullptr);
    }

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    /** @brief The layout of the pipelines, for the push constants. */
    VkPipelineLayout layout() const { return layout_; }

    /** @brief The vertices of all meshes. */
    VkBuffer vertices() const { return vertices_->buffer(); }

    /** @brief The indices of all meshes. */
    VkBuffer indices() const { return indices_->buffer(); }

    /** @brief Where a mesh is in vertices() and indices(). */
    const Part& part(MeshKind m) const { return parts_[m]; }

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
        const VkShaderModule vert = make_shader(device, pipe_vert);
        const VkShaderModule frag = make_shader(device, pipe_frag);
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

        // Binding 0: the vertices of the mesh. Binding 1: the instances.
        const VkVertexInputBindingDescription bindings[] = {
            {.binding = 0, .stride = sizeof(Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX},
            {.binding = 1, .stride = sizeof(Instance), .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE},
        };
        const VkVertexInputAttributeDescription attributes[] = {
            {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, position)},
            {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, normal)},
            {.location = 2, .binding = 1, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Instance, axes)},
            {.location = 3, .binding = 1, .format = VK_FORMAT_R32G32B32_SFLOAT,
             .offset = offsetof(Instance, axes) + 3 * sizeof(float)},
            {.location = 4, .binding = 1, .format = VK_FORMAT_R32G32B32_SFLOAT,
             .offset = offsetof(Instance, axes) + 6 * sizeof(float)},
            {.location = 5, .binding = 1, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Instance, position)},
            {.location = 6, .binding = 1, .format = VK_FORMAT_R32_UINT, .offset = offsetof(Instance, colour)},
        };
        const VkPipelineVertexInputStateCreateInfo input = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 2,
            .pVertexBindingDescriptions = bindings,
            .vertexAttributeDescriptionCount = 7,
            .pVertexAttributeDescriptions = attributes,
        };
        const VkPipelineInputAssemblyStateCreateInfo assembly = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        };
        const VkPipelineViewportStateCreateInfo viewport = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };
        // glEnable(GL_CULL_FACE) with the front faces counter-clockwise, as in pipes.c.
        const VkPipelineRasterizationStateCreateInfo raster = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .lineWidth = 1.0f,
        };
        // Every sample shaded, when the device can, so that the small
        // highlights of the far pipes do not shimmer.
        const VkPipelineMultisampleStateCreateInfo multisample = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = gpu_.samples(),
            .sampleShadingEnable = gpu_.sample_rate_shading() ? VK_TRUE : VK_FALSE,
            .minSampleShading = 1.0f,
        };
        const VkPipelineDepthStencilStateCreateInfo depth = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS,
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
    /** @brief The device. */
    Gpu& gpu_;
    /** @brief The vertices of the meshes, one after the other. */
    std::unique_ptr<Buffer> vertices_;
    /** @brief The indices of the meshes, one after the other, each from 0 for its own vertices. */
    std::unique_ptr<Buffer> indices_;
    /** @brief Where each mesh is. */
    std::array<Part, MESHES> parts_;
    /** @brief The layout of the pipelines. */
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    /** @brief The pipelines, with the format of the images they draw into. */
    std::vector<std::pair<VkFormat, VkPipeline>> pipelines_;
};

namespace {

/** @brief The pipes on one surface. */
class PipesScene : public Scene {
public:
    /**
     * @param gpu      The device.
     * @param renderer The meshes and the pipeline layout; it must outlive the scene.
     * @param pipeline The pipeline for the format of the surface.
     * @param options  The options.
     * @param seed     The seed of the walk.
     * @param rotation The angle about the Y axis to start at, in degrees; only rotatepipes turns the scene.
     */
    PipesScene(Gpu& gpu, const Renderer& renderer, VkPipeline pipeline, const PipesOptions& options, uint32_t seed,
               float rotation)
        : renderer_(renderer)
        , pipeline_(pipeline)
        , options_(options)
        , walk_(seed, options.walk)
        , rotation_(rotation)
    {
        // The most instances of a screenful: per system, those of size + 1
        // steps and the start and the cap. The regions of the meshes are
        // laid out per screenful, in upload().
        const VkDeviceSize steps = static_cast<VkDeviceSize>(options.walk.size + 1) * options.walk.cycles;
        const VkDeviceSize size = (MOST_INSTANCES_PER_STEP * steps + 2 * options.walk.cycles) * sizeof(Instance);
        for (auto& buffer : buffers_) {
            buffer = std::make_unique<Buffer>(gpu, size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        }
        load(walk_.next());
    }

    void draw(VkCommandBuffer cmd, VkExtent2D extent) override
    {
        const float between = advance();
        if (fresh_) {
            upload();
        }

        // The transformations of draw_pipes, in its order.
        Mat4 modelview = translate(0.0f, 0.0f, options_.fisheye ? -FISHEYE_DISTANCE : -DISTANCE);
        if (options_.rotatepipes) {
            modelview = modelview * rotate(rotation_ + ROTATION_STEP * between, 0.0f, 1.0f, 0.0f);
        }
        modelview = modelview * scale(SCENE_SCALE, SCENE_SCALE, SCENE_SCALE);
        if (fade_ > 0) {
            const float fade = std::max(static_cast<float>(fade_) - FADE_STEP * between, 0.0f);
            const float s = fade * fade / (FADE_START * FADE_START);
            if (s <= 0.0f) {
                return;
            }
            modelview = modelview * scale(s, s, s) * rotate(90.0f * (1.0f - fade / FADE_START), 1.0f, 0.0f, 0.1f);
        }
        const Transform transform = {
            .modelview = modelview,
            .projection = perspective(FOVY, static_cast<float>(extent.width) / static_cast<float>(extent.height),
                                      NEAR, FAR),
        };

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
        vkCmdPushConstants(cmd, renderer_.layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof transform, &transform);
        vkCmdBindIndexBuffer(cmd, renderer_.indices(), 0, VK_INDEX_TYPE_UINT32);
        for (std::size_t m = 0; m < MESHES; m++) {
            const uint32_t count = counts_[m][shown_];
            if (count == 0) {
                continue;
            }
            const VkBuffer buffers[] = {renderer_.vertices(), buffers_[current_]->buffer()};
            const VkDeviceSize offsets[] = {0, offsets_[current_][m]};
            vkCmdBindVertexBuffers(cmd, 0, 2, buffers, offsets);
            const Renderer::Part& part = renderer_.part(static_cast<MeshKind>(m));
            vkCmdDrawIndexed(cmd, part.index_count, count, part.first_index, part.vertex_offset, 0);
        }
    }

private:
    /**
     * @brief Takes the steps of the time since the last frame.
     * @return How far the scene is from the last step to the next, from 0 to 1.
     */
    float advance()
    {
        using std::chrono::steady_clock;
        const steady_clock::time_point now = steady_clock::now();
        if (last_ == steady_clock::time_point()) {
            last_ = now;
        }
        lag_ += std::chrono::duration_cast<std::chrono::microseconds>(now - last_).count();
        last_ = now;
        int64_t steps = lag_ / options_.delay;
        lag_ -= steps * options_.delay;
        steps = std::min(steps, MAX_STEPS_PER_FRAME);
        for (int64_t i = 0; i < steps; i++) {
            step();
        }
        return static_cast<float>(lag_) / static_cast<float>(options_.delay);
    }

    /** @brief One step, a frame of draw_pipes: show the next step of the walk, or go on with the restart. */
    void step()
    {
        rotation_ = std::fmod(rotation_ + ROTATION_STEP, 360.0f);
        if (fade_ > 0) {
            fade_ -= FADE_STEP;
            if (fade_ <= 0) {
                fade_ = 0;
                load(walk_.next());
            }
        } else if (shown_ < steps()) {
            shown_++;
        } else {
            fade_ = FADE_START;
        }
    }

    /** @brief The steps of the screenful, all systems one after the other. */
    std::size_t steps() const { return counts_[TUBE].size() - 1; }

    /** @brief Makes the instances of a screenful, for upload(), and shows none of its steps. */
    void load(const Screenful& screenful)
    {
        for (std::size_t m = 0; m < MESHES; m++) {
            instances_[m].clear();
            counts_[m].assign(1, 0);
        }
        for (const System& system : screenful.systems) {
            for (const Step& step : system.steps) {
                for (const Piece& piece : step.pieces) {
                    add_instances(piece, system.colour, instances_);
                }
                for (std::size_t m = 0; m < MESHES; m++) {
                    counts_[m].push_back(static_cast<uint32_t>(instances_[m].size()));
                }
            }
        }
        shown_ = 0;
        fresh_ = true;
    }

    /**
     * @brief Writes the instances of the screenful into the buffer that the last frame did not use,
     *        the meshes one after the other.
     *
     * The frames before the last are done when draw() is called, so that
     * buffer is free.
     */
    void upload()
    {
        current_ = 1 - current_;
        auto* data = static_cast<unsigned char*>(buffers_[current_]->data());
        VkDeviceSize offset = 0;
        for (std::size_t m = 0; m < MESHES; m++) {
            offsets_[current_][m] = offset;
            std::memcpy(data + offset, instances_[m].data(), instances_[m].size() * sizeof(Instance));
            offset += instances_[m].size() * sizeof(Instance);
        }
        fresh_ = false;
    }

    /** @brief The meshes and the pipeline layout. */
    const Renderer& renderer_;
    /** @brief The pipeline. */
    VkPipeline pipeline_;
    /** @brief The options. */
    PipesOptions options_;
    /** @brief The walk of the screenfuls. */
    Walk walk_;
    /** @brief The angle about the Y axis, in degrees, at the last step. */
    float rotation_;
    /** @brief The restart, fadeout of pipes.c: from 100 down to 0 while the scene shrinks; 0 while it builds. */
    int fade_ = 0;
    /** @brief The steps of the screenful shown, all systems one after the other. */
    std::size_t shown_ = 0;
    /** @brief The instances of the screenful, per mesh, in the order of the steps. */
    Instances instances_;
    /** @brief Per mesh, the instances of the first n steps at n, from 0 to all steps. */
    std::array<std::vector<uint32_t>, MESHES> counts_;
    /** @brief Whether the instances are not in a buffer yet. */
    bool fresh_ = false;
    /** @brief Two buffers of instances, each with a region per mesh at offsets_; one written while the other is drawn. */
    std::array<std::unique_ptr<Buffer>, 2> buffers_;
    /** @brief Where the region of each mesh starts in each buffer. */
    std::array<std::array<VkDeviceSize, MESHES>, 2> offsets_{};
    /** @brief The buffer of the screenful shown. */
    std::size_t current_ = 0;
    /** @brief When the last frame was drawn; the epoch before the first. */
    std::chrono::steady_clock::time_point last_;
    /** @brief The time since the last step, in µs. */
    int64_t lag_ = 0;
};

} // namespace

Pipes::Pipes(const PipesOptions& options) : options_(options) {}

Pipes::~Pipes() = default;

std::unique_ptr<Scene> Pipes::make_scene(Gpu& gpu, VkFormat format)
{
    if (renderer_ == nullptr) {
        renderer_ = std::make_unique<Renderer>(gpu);
    }
    std::random_device device;
    // A seed of its own for every monitor, and the random start of the turn of pipes.c.
    const uint32_t seed = device();
    const float rotation = static_cast<float>(device() % 180);
    return std::make_unique<PipesScene>(gpu, *renderer_, renderer_->pipeline(format), options_, seed, rotation);
}
