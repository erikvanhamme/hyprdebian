/**
 * @file window.hpp
 * @brief The Wayland side of hd-menu.
 *
 * A layer-shell surface over the whole output, the shared memory buffers
 * that the renderer draws into, and the keyboard.
 *
 * How a frame gets on the screen:
 *
 * 1. The compositor sends a configure event of the layer surface with the
 *    size of the surface, in logical pixels. hd-menu acks it.
 * 2. hd-menu takes a free buffer: memory shared with the compositor (a
 *    memfd, mapped by both), width * scale by height * scale pixels of 32
 *    bit ARGB. cairo draws into that memory directly.
 * 3. hd-menu attaches the buffer to the surface, marks it damaged and
 *    commits. From then on the buffer belongs to the compositor, until it
 *    sends a release event of the buffer.
 *
 * There are two buffers, so that hd-menu can draw the next frame into one
 * while the compositor still reads the other.
 */
#pragma once

#include <array>
#include <cairo.h>
#include <cstddef>
#include <cstdint>

struct wl_buffer;
struct wl_compositor;
struct wl_display;
struct wl_keyboard;
struct wl_registry;
struct wl_seat;
struct wl_shm;
struct wl_surface;
struct zwlr_layer_shell_v1;
struct zwlr_layer_surface_v1;

/**
 * @brief What the window tells the rest of hd-menu.
 *
 * Sizes are in logical pixels.
 */
class WindowHandler {
public:
    virtual ~WindowHandler() = default;

    /**
     * @brief Draws a complete frame.
     * @param cr     The cairo context of the buffer, scaled for the buffer
     *               scale so that coordinates are logical pixels.
     * @param width  The width of the surface.
     * @param height The height of the surface.
     */
    virtual void draw(cairo_t* cr, int width, int height) = 0;

    /**
     * @brief Handles a key press.
     * @param key The Linux key code (KEY_UP, ... of linux/input-event-codes.h):
     *            the physical key, whatever the keyboard layout.
     */
    virtual void key(uint32_t key) = 0;

    /** @brief Handles the compositor closing the surface. */
    virtual void closed() = 0;
};

/** @brief The surface of the menu, from the connection to the compositor to the frames on the screen. */
class Window {
public:
    /**
     * @brief Connects to the compositor and asks for the surface.
     *
     * The first configure, and with it the first frame, comes in run().
     *
     * @param handler Gets the frames to draw, the keys and the close; it
     *                must outlive the window.
     * @throws std::runtime_error When there is no Wayland compositor, or it
     *         lacks wl_compositor 4, wl_shm, wl_seat 5 or zwlr_layer_shell_v1 4.
     */
    explicit Window(WindowHandler& handler);

    /** @brief Destroys the surface and the buffers and disconnects. */
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    /**
     * @brief Handles the events of the compositor until quit().
     * @throws std::runtime_error When the connection to the compositor is lost.
     */
    void run();

    /** @brief Makes run() return after the event it is handling. */
    void quit() { running_ = false; }

    /** @brief Draws a new frame, now or as soon as a buffer is free. */
    void redraw();

    /**
     * @brief The width of the surface.
     * @return The width in logical pixels; 0 before the first configure.
     */
    int width() const { return width_; }

    /**
     * @brief The height of the surface.
     * @return The height in logical pixels; 0 before the first configure.
     */
    int height() const { return height_; }

private:
    /** @brief A buffer of shared memory that cairo draws into and the compositor shows. */
    struct Buffer {
        /** @brief The window, for the release event. */
        Window* window = nullptr;
        /** @brief The buffer of the compositor; null when not created yet. */
        wl_buffer* buffer = nullptr;
        /** @brief The cairo image surface on the same memory. */
        cairo_surface_t* cairo = nullptr;
        /** @brief The mapped memory. */
        void* data = nullptr;
        /** @brief The size of the memory, in bytes. */
        std::size_t size = 0;
        /** @brief Whether the compositor still has the buffer, from the commit until the release. */
        bool busy = false;
    };

    /**
     * @brief Binds a global of the registry that hd-menu uses.
     * @param registry  The registry.
     * @param name      The name of the global.
     * @param interface The interface of the global.
     * @param version   The highest version the compositor has.
     */
    void bind_global(wl_registry* registry, uint32_t name, const char* interface, uint32_t version);

    /**
     * @brief Acks a configure of the layer surface and draws at the new size.
     * @param serial The serial of the configure, to ack.
     * @param width  The width of the surface, in logical pixels.
     * @param height The height of the surface, in logical pixels.
     */
    void configure(uint32_t serial, uint32_t width, uint32_t height);

    /**
     * @brief Changes the scale of the buffers and draws again.
     * @param scale The buffer scale the compositor prefers.
     */
    void set_scale(int scale);

    /** @brief Draws and commits a frame when one is due and a buffer is free. */
    void draw_if_possible();

    /**
     * @brief Finds a buffer that the compositor does not have, creating it if needed.
     * @return The buffer, or null when both are busy.
     * @throws std::runtime_error When a buffer cannot be created.
     */
    Buffer* free_buffer();

    /**
     * @brief Creates the shared memory of b at the current size and scale.
     * @param b The buffer to create.
     * @throws std::runtime_error When the memory cannot be allocated or mapped.
     */
    void create_buffer(Buffer& b);

    /** @brief Destroys both buffers, for a new size or scale. */
    void destroy_buffers();

    /** @brief Gets the frames to draw, the keys and the close. */
    WindowHandler& handler_;
    /** @brief Whether run() goes on. */
    bool running_ = true;

    /** @brief The connection to the compositor. */
    wl_display* display_ = nullptr;
    /** @brief The registry of the globals of the compositor. */
    wl_registry* registry_ = nullptr;
    /** @brief Makes the surface. */
    wl_compositor* compositor_ = nullptr;
    /** @brief Makes the shared memory buffers. */
    wl_shm* shm_ = nullptr;
    /** @brief The seat, for the keyboard. */
    wl_seat* seat_ = nullptr;
    /** @brief The keyboard of the seat. */
    wl_keyboard* keyboard_ = nullptr;
    /** @brief Gives the surface the layer-shell role. */
    zwlr_layer_shell_v1* layer_shell_ = nullptr;

    /** @brief The surface of the menu. */
    wl_surface* surface_ = nullptr;
    /** @brief The layer-shell role of the surface. */
    zwlr_layer_surface_v1* layer_surface_ = nullptr;
    /** @brief The two buffers, drawn into in turn. */
    std::array<Buffer, 2> buffers_;
    /** @brief Whether the first configure has come. */
    bool configured_ = false;
    /** @brief Whether a new frame is due. */
    bool dirty_ = false;
    /** @brief The width of the surface, in logical pixels. */
    int width_ = 0;
    /** @brief The height of the surface, in logical pixels. */
    int height_ = 0;
    /** @brief The buffer scale: pixels of the buffer per logical pixel. */
    int scale_ = 1;
};
