/**
 * @file swapchain.hpp
 * @brief The images of one surface, and how a frame gets on it.
 *
 * Each surface has a swapchain of its own, with images of 8 bit UNORM, not
 * sRGB: the shaders light in gamma space, as OpenGL 1.x did, and write the
 * colour as it goes on the screen. It presents with FIFO, at the vsync of
 * the monitor.
 *
 * A frame:
 *
 * 1. Wait until the frame in flight before the last is done, so that its
 *    command buffer and semaphore can be used again.
 * 2. Acquire an image of the swapchain.
 * 3. Record: clear the multisampled colour and depth attachments to black
 *    and the far plane, let the scene draw into them, and resolve the
 *    colour into the image of the swapchain.
 * 4. Submit, which waits for the acquire, and present, which waits for the
 *    submit.
 *
 * With one sample per pixel, the scene draws into the image of the
 * swapchain directly.
 */
#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <vulkan/vulkan.h>

class Gpu;
struct wl_display;
struct wl_surface;

/** @brief The swapchain of one surface, with its attachments and the frames in flight. */
class Swapchain {
public:
    /** @brief Records the drawing of the scene, between the clear and the resolve. */
    using Draw = std::function<void(VkCommandBuffer cmd, VkExtent2D extent)>;

    /**
     * @brief Creates the Vulkan surface of a Wayland surface and picks its format.
     *
     * The swapchain itself comes with the first resize().
     *
     * @param gpu     The device; it must outlive the swapchain.
     * @param display The connection to the compositor.
     * @param surface The Wayland surface; it must outlive the swapchain.
     * @throws std::runtime_error When the surface cannot be made or has no UNORM format.
     */
    Swapchain(Gpu& gpu, wl_display* display, wl_surface* surface);

    /** @brief Waits until the device is idle and destroys the swapchain and the surface. */
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    /** @brief The format of the images, for the pipelines of the scene. */
    VkFormat format() const { return format_; }

    /**
     * @brief Sets the size of the images; the next draw() makes them.
     * @param width  The width of the surface, in pixels.
     * @param height The height of the surface, in pixels.
     */
    void resize(uint32_t width, uint32_t height);

    /**
     * @brief Draws a frame and presents it.
     * @param draw Records the drawing of the scene.
     * @return Whether the frame was presented; false when the swapchain was
     *         out of date, it is made again for the next draw().
     * @throws std::runtime_error When a Vulkan call fails.
     */
    bool draw(const Draw& draw);

private:
    /** @brief The command buffer and the synchronisation of one frame in flight. */
    struct Frame {
        /** @brief The commands of the frame. */
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        /** @brief Signalled when the image is acquired; the submit waits for it. */
        VkSemaphore acquired = VK_NULL_HANDLE;
        /** @brief Signalled when the commands are done, so that the frame can be used again. */
        VkFence done = VK_NULL_HANDLE;
    };

    /** @brief An image of the device with its memory and view: an attachment. */
    struct Attachment {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
    };

    /**
     * @brief Makes the swapchain at the size of resize(), with its views and attachments.
     * @throws std::runtime_error When a Vulkan call fails.
     */
    void create();

    /** @brief Destroys the views and attachments; keeps the swapchain, to pass as oldSwapchain. */
    void destroy_images();

    /**
     * @brief Makes an attachment at the size of the swapchain.
     * @param format The format of the image.
     * @param usage  How the image is used.
     * @param aspect The aspect of the view.
     * @param samples The samples per pixel.
     * @throws std::runtime_error When a Vulkan call fails.
     */
    Attachment make_attachment(VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect,
                               VkSampleCountFlagBits samples);

    /**
     * @brief Destroys an attachment.
     * @param a The attachment; it is reset.
     */
    void destroy_attachment(Attachment& a);

    /**
     * @brief Records a frame into the command buffer of the current frame.
     * @param image The index of the swapchain image.
     * @param draw  Records the drawing of the scene.
     */
    void record(uint32_t image, const Draw& draw);

    /** @brief The device. */
    Gpu& gpu_;
    /** @brief The Vulkan surface of the Wayland surface. */
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    /** @brief The format of the images. */
    VkFormat format_ = VK_FORMAT_UNDEFINED;
    /** @brief The colour space that goes with format_. */
    VkColorSpaceKHR color_space_ = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    /** @brief The swapchain; null until the first draw() after a resize(). */
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    /** @brief The size the swapchain has, or gets in create(). */
    VkExtent2D extent_ = {0, 0};
    /** @brief Whether the next draw() makes the swapchain again. */
    bool stale_ = true;
    /** @brief The images of the swapchain. */
    std::vector<VkImage> images_;
    /** @brief A view of each image. */
    std::vector<VkImageView> views_;
    /**
     * @brief Signalled when the commands of the frame on an image are done; the present waits for it.
     *
     * One per image, not per frame: the present may still wait on it when
     * the frame is used again, but not when its image comes back.
     */
    std::vector<VkSemaphore> rendered_;
    /** @brief The multisampled colour attachment, resolved into the image; none with 1 sample. */
    Attachment color_;
    /** @brief The depth attachment. */
    Attachment depth_;
    /** @brief The pool of the command buffers. */
    VkCommandPool pool_ = VK_NULL_HANDLE;
    /** @brief The frames in flight. */
    std::vector<Frame> frames_;
    /** @brief The frame to use next. */
    std::size_t frame_ = 0;
};
