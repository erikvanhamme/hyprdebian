/**
 * @file screen.hpp
 * @brief The Wayland side of the screensavers.
 *
 * A layer-shell surface on the overlay layer of every output, over the
 * whole output and with the keyboard to itself, each with a swapchain and a
 * scene of its own. Outputs that come while it runs get a surface; those
 * that go lose theirs. The screensaver ends on input: a key press, pointer
 * motion, a button, a scroll or a touch. The enter of the pointer alone does
 * not count, as the surface appears under a mouse that stands still; the
 * cursor is hidden on it. It also ends on SIGTERM and SIGINT.
 *
 * With --window it is one ordinary window instead, an xdg-toplevel that
 * input does not end; Escape or closing the window does.
 *
 * A frame of a surface is drawn when the compositor wants one: the first
 * after the configure, and each next one after the frame callback of the
 * one before. So FIFO never waits on a surface that is not shown, and the
 * monitors of other refresh rates do not hold each other up.
 */
#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

class Gpu;
class Swapchain;
struct wl_callback;
struct wl_compositor;
struct wl_display;
struct wl_keyboard;
struct wl_output;
struct wl_pointer;
struct wl_registry;
struct wl_seat;
struct wl_surface;
struct wl_touch;
struct xdg_surface;
struct xdg_toplevel;
struct xdg_wm_base;
struct zwlr_layer_shell_v1;
struct zwlr_layer_surface_v1;

/** @brief What a screensaver draws on one surface. */
class Scene {
public:
    virtual ~Scene() = default;

    /**
     * @brief Records the drawing of a frame.
     *
     * The attachments are cleared to black and the far plane, and the
     * viewport and scissor set to the whole surface, before.
     *
     * @param cmd    The command buffer, within the rendering.
     * @param extent The size of the surface, in pixels.
     */
    virtual void draw(VkCommandBuffer cmd, VkExtent2D extent) = 0;
};

/** @brief Makes the scenes of a screensaver. */
class SceneMaker {
public:
    virtual ~SceneMaker() = default;

    /**
     * @brief Makes the scene of a new surface.
     * @param gpu    The device.
     * @param format The format of the images of the surface.
     * @return The scene; it is destroyed before the device.
     */
    virtual std::unique_ptr<Scene> make_scene(Gpu& gpu, VkFormat format) = 0;
};

/** @brief The connection to the compositor, the surfaces, the input and the event loop. */
class Screen {
public:
    /**
     * @brief Connects to the compositor and binds what the screensaver needs.
     * Blocks SIGTERM and SIGINT, which run() reads from a signalfd. It
     * comes before the Vulkan device, whose threads take over the blocked
     * signals.
     *
     * @param window Whether to show one window instead of covering the outputs.
     * @throws std::runtime_error When there is no Wayland compositor, it has
     *         no outputs, or it lacks wl_compositor 4 or, without window,
     *         zwlr_layer_shell_v1 4 or, with window, xdg_wm_base.
     */
    explicit Screen(bool window);

    /** @brief Destroys the surfaces and disconnects. */
    ~Screen();

    Screen(const Screen&) = delete;
    Screen& operator=(const Screen&) = delete;

    /** @brief The connection, for the Vulkan device. */
    wl_display* display() const { return display_; }

    /**
     * @brief Makes the surfaces and handles the events until the screensaver ends.
     * @param gpu   The device; it must outlive the screen.
     * @param maker Makes the scenes; it must outlive the screen.
     * @throws std::runtime_error When the connection to the compositor is
     *         lost or a Vulkan call fails.
     */
    void run(Gpu& gpu, SceneMaker& maker);

private:
    struct Surface;
    struct Output;
    struct Seat;

    /**
     * @brief Binds a global of the registry that the screensaver uses.
     * @param name      The name of the global.
     * @param interface The interface of the global.
     * @param version   The highest version the compositor has.
     */
    void bind_global(uint32_t name, const char* interface, uint32_t version);

    /**
     * @brief Handles a global going away: an output loses its surface, a seat its devices.
     * @param name The name of the global.
     */
    void remove_global(uint32_t name);

    /**
     * @brief Makes a surface and its role, on an output or as the window.
     * @param output The output to cover; null for the window.
     * @return The surface; its swapchain comes with the first configure.
     */
    std::unique_ptr<Surface> make_surface(wl_output* output);

    /**
     * @brief Sets the size of a surface, from a configure of its role.
     * @param s      The surface.
     * @param width  The width, in pixels.
     * @param height The height, in pixels.
     */
    void configure(Surface& s, uint32_t width, uint32_t height);

    /**
     * @brief Draws and presents a frame of a surface, and asks for the next.
     * @param s The surface.
     */
    void draw(Surface& s);

    /** @brief Destroys every surface, with its swapchain and scene, before the device goes. */
    void destroy_surfaces();

    /** @brief Handles input that ends the screensaver; nothing with --window. */
    void input();

    /** @brief Ends run() after the event it is handling. */
    void quit() { running_ = false; }

    /** @brief Whether this is the window of --window. */
    bool window_mode_;
    /** @brief Whether run() goes on. */
    bool running_ = true;
    /** @brief The device; null until run(). */
    Gpu* gpu_ = nullptr;
    /** @brief Makes the scenes; null until run(). */
    SceneMaker* maker_ = nullptr;

    /** @brief The connection to the compositor. */
    wl_display* display_ = nullptr;
    /** @brief The registry of the globals of the compositor. */
    wl_registry* registry_ = nullptr;
    /** @brief Makes the surfaces. */
    wl_compositor* compositor_ = nullptr;
    /** @brief Gives the surfaces the layer-shell role; null with --window. */
    zwlr_layer_shell_v1* layer_shell_ = nullptr;
    /** @brief Gives the window its role; null without --window. */
    xdg_wm_base* wm_base_ = nullptr;
    /** @brief The outputs, each with its surface while run() goes on. */
    std::vector<std::unique_ptr<Output>> outputs_;
    /** @brief The seats, with their keyboard, pointer and touch. */
    std::vector<std::unique_ptr<Seat>> seats_;
    /** @brief The window of --window; null without it. */
    std::unique_ptr<Surface> window_;
    /** @brief The signalfd of SIGTERM and SIGINT, polled in run() next to the connection. */
    int signal_fd_ = -1;
};
