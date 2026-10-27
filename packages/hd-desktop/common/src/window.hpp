/**
 * @file window.hpp
 * @brief The Wayland side of hd-menu and hd-message.
 *
 * A layer-shell surface over the whole output, the shared memory buffers
 * that the renderer draws into, and the keyboard.
 *
 * How a frame gets on the screen:
 *
 * 1. The compositor sends a configure event of the layer surface with the
 *    size of the surface, in logical pixels. The program acks it.
 * 2. The program takes a free buffer: memory shared with the compositor (a
 *    memfd, mapped by both), width * scale by height * scale pixels of 32
 *    bit ARGB. cairo draws into that memory directly.
 * 3. The program attaches the buffer to the surface, marks it damaged and
 *    commits. From then on the buffer belongs to the compositor, until it
 *    sends a release event of the buffer.
 *
 * There are two buffers, so that the program can draw the next frame into one
 * while the compositor still reads the other.
 */
#pragma once

#include <array>
#include <cairo.h>
#include <cstddef>
#include <cstdint>
#include <xkbcommon/xkbcommon.h>

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
 * @brief What the window tells the rest of the program.
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
     * @param sym The keysym of the key in the keyboard layout, with the
     *            modifiers that are down (XKB_KEY_q, ...); XKB_KEY_NoSymbol
     *            when the compositor sent no keymap that could be read.
     */
    virtual void key(uint32_t key, xkb_keysym_t sym) = 0;

    /** @brief Handles the compositor closing the surface. */
    virtual void closed() = 0;

    /** @brief Handles the end of the timer of Window::start_timer(); nothing by default. */
    virtual void timeout() {}
};

/** @brief The surface of the program, from the connection to the compositor to the frames on the screen. */
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
     * @brief Calls WindowHandler::timeout() once, after a time.
     *
     * A timer that is running starts again from now.
     *
     * @param ms Milliseconds from now, at least 1.
     */
    void start_timer(int ms);

    /** @brief Stops the timer of start_timer(), if it is running. */
    void stop_timer();

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
     * @brief Binds a global of the registry that the program uses.
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
     * @brief Reads the keymap of the keyboard and closes its file descriptor.
     *
     * A keymap that cannot be read leaves the keysyms as they were; the keys
     * that go by key code work without one.
     *
     * @param format The format of the keymap.
     * @param fd     The file descriptor of the keymap.
     * @param size   The size of the keymap, in bytes.
     */
    void set_keymap(uint32_t format, int fd, uint32_t size);

    /**
     * @brief Handles a key going down or up, and starts or stops its repeat.
     * @param key     The Linux key code.
     * @param pressed Whether the key went down.
     */
    void key_event(uint32_t key, bool pressed);

    /**
     * @brief Sets the repeat of the keyboard, as the compositor tells it.
     * @param rate  Repeats per second; 0 for no repeat.
     * @param delay Milliseconds from the press to the first repeat.
     */
    void set_repeat(int32_t rate, int32_t delay);

    /** @brief Stops the repeat of the key that is held, if any. */
    void stop_repeat();

    /** @brief Handles the expirations of the repeat timer: one key press per expiration. */
    void repeat();

    /**
     * @brief The keysym of a key in the keymap, with the current modifiers.
     * @param key The Linux key code.
     * @return The keysym; XKB_KEY_NoSymbol without a keymap.
     */
    xkb_keysym_t keysym(uint32_t key) const;

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
    /** @brief Compiles the keymap. */
    xkb_context* xkb_context_ = nullptr;
    /** @brief The keymap of the keyboard; null until the compositor sends one. */
    xkb_keymap* xkb_keymap_ = nullptr;
    /** @brief The keymap with the modifiers that are down; null without a keymap. */
    xkb_state* xkb_state_ = nullptr;
    /** @brief The timerfd of the key repeat, polled in run() next to the connection. */
    int repeat_fd_ = -1;
    /** @brief The timerfd of start_timer(), polled in run() next to the connection. */
    int timer_fd_ = -1;
    /** @brief The key that repeats while it is held; 0 for none. */
    uint32_t repeat_key_ = 0;
    /**
     * @brief Repeats per second; 0 for no repeat.
     *
     * Until the compositor sends its repeat_info, the defaults of Hyprland.
     */
    int32_t repeat_rate_ = 25;
    /** @brief Milliseconds from the press to the first repeat. */
    int32_t repeat_delay_ = 600;
    /** @brief Gives the surface the layer-shell role. */
    zwlr_layer_shell_v1* layer_shell_ = nullptr;

    /** @brief The surface of the program. */
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
