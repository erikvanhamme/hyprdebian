#include "screen.hpp"

#include "gpu.hpp"
#include "program.hpp"
#include "swapchain.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <sys/signalfd.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>
#include <wayland-client.h>

#include "linux-dmabuf-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"
// The header is C, with an argument named namespace, a keyword in C++. It
// comes after the other headers, so that the define touches only it.
#define namespace namespace_
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#undef namespace

namespace {

/** @brief The size of the window of --window when the compositor leaves it to the program. */
constexpr uint32_t WINDOW_WIDTH = 1280;
constexpr uint32_t WINDOW_HEIGHT = 720;

/** @brief How long run() waits before it tries again to draw a frame that was not presented, in ms. */
constexpr int RETRY_MS = 16;

/**
 * @brief How long before its turn on the clock a frame is drawn, with a frame rate.
 *
 * FIFO shows it at the first refresh after it is presented; drawn a little
 * early, it is ready for the refresh at its turn and not the one after.
 */
constexpr std::chrono::microseconds FRAME_LEAD{2000};

void fail(const std::string& what)
{
    throw std::runtime_error(what);
}

} // namespace

// libwayland calls C function pointers, with the pointer given to
// add_listener as data. The listeners here are captureless lambdas, which
// convert to function pointers; defined in member functions, they may call
// the private members of Screen.

/** @brief A surface that the screensaver draws on: one per output, or the window. */
struct Screen::Surface {
    /** @brief The surface. */
    wl_surface* surface = nullptr;
    /** @brief The layer-shell role, on an output; null for the window. */
    zwlr_layer_surface_v1* layer = nullptr;
    /** @brief The xdg-shell role of the window; null on an output. */
    xdg_surface* xdg = nullptr;
    /** @brief The toplevel of the window; null on an output. */
    xdg_toplevel* toplevel = nullptr;
    /** @brief The output the surface is on: that of the layer surface, or the last one the window entered. */
    wl_output* output = nullptr;
    /** @brief The frame callback of the last frame; null when it is done. */
    wl_callback* frame = nullptr;
    /** @brief The swapchain; null until the first configure. */
    std::unique_ptr<Swapchain> swapchain;
    /** @brief The scene; null until the first configure. */
    std::unique_ptr<Scene> scene;
    /** @brief The size the toplevel asks for; 0 when it leaves it to the program. */
    uint32_t pending_width = 0;
    uint32_t pending_height = 0;
    /** @brief The size of the surface, in pixels; 0 until the first configure. */
    uint32_t width = 0;
    uint32_t height = 0;
    /** @brief Whether a frame may be drawn: after a configure, and when the frame callback of the last is done. */
    bool due = false;
    /** @brief The turn of the next frame on the clock, with a frame rate; it is drawn FRAME_LEAD before. */
    std::chrono::steady_clock::time_point turn;
    /** @brief Whether the compositor closed the layer surface; run() destroys it. */
    bool closed = false;

    Surface() = default;
    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;

    /**
     * @brief Destroys the swapchain, which waits until the device is idle,
     *        the scene, and then the surface and its role.
     */
    ~Surface()
    {
        swapchain.reset();
        scene.reset();
        if (frame != nullptr) {
            wl_callback_destroy(frame);
        }
        if (layer != nullptr) {
            zwlr_layer_surface_v1_destroy(layer);
        }
        if (toplevel != nullptr) {
            xdg_toplevel_destroy(toplevel);
        }
        if (xdg != nullptr) {
            xdg_surface_destroy(xdg);
        }
        if (surface != nullptr) {
            wl_surface_destroy(surface);
        }
    }
};

/** @brief An output of the compositor, with its surface while run() goes on. */
struct Screen::Output {
    /** @brief The name of the global. */
    uint32_t name = 0;
    /** @brief The output. */
    wl_output* output = nullptr;
    /** @brief The version of the output, for its release. */
    uint32_t version = 0;
    /** @brief The refresh rate of its current mode, in mHz; 0 until the output sends it. */
    int refresh = 0;
    /** @brief The surface on the output; null before run(). */
    std::unique_ptr<Surface> surface;

    Output() = default;
    Output(const Output&) = delete;
    Output& operator=(const Output&) = delete;

    ~Output()
    {
        surface.reset();
        if (version >= WL_OUTPUT_RELEASE_SINCE_VERSION) {
            wl_output_release(output);
        } else {
            wl_output_destroy(output);
        }
    }
};

/** @brief A seat of the compositor, with the devices it has. */
struct Screen::Seat {
    /** @brief The screen, for the input. */
    Screen* screen = nullptr;
    /** @brief The name of the global. */
    uint32_t name = 0;
    /** @brief The seat. */
    wl_seat* seat = nullptr;
    /** @brief The keyboard; null when the seat has none. */
    wl_keyboard* keyboard = nullptr;
    /** @brief The pointer; null when the seat has none. */
    wl_pointer* pointer = nullptr;
    /** @brief The touch screen; null when the seat has none. */
    wl_touch* touch = nullptr;
    /** @brief Where the pointer is on the surface it is on, from its enter. */
    wl_fixed_t x = 0;
    wl_fixed_t y = 0;

    Seat() = default;
    Seat(const Seat&) = delete;
    Seat& operator=(const Seat&) = delete;

    /**
     * @brief Gets and releases the devices as the seat has them.
     * @param capabilities The capabilities of the seat.
     */
    void set_capabilities(uint32_t capabilities);

    ~Seat()
    {
        set_capabilities(0);
        wl_seat_release(seat);
    }
};

void Screen::Seat::set_capabilities(uint32_t capabilities)
{
    // The keyboard. A key press ends the screensaver; in the window only
    // Escape, which goes by key code and needs no keymap.
    static const wl_keyboard_listener keyboard_listener = {
        .keymap = [](void*, wl_keyboard*, uint32_t, int32_t fd, uint32_t) { close(fd); },
        .enter = [](void*, wl_keyboard*, uint32_t, wl_surface*, wl_array*) {},
        .leave = [](void*, wl_keyboard*, uint32_t, wl_surface*) {},
        .key = [](void* data, wl_keyboard*, uint32_t, uint32_t, uint32_t key, uint32_t state) {
            Screen* screen = static_cast<Seat*>(data)->screen;
            if (state != WL_KEYBOARD_KEY_STATE_PRESSED) {
                return;
            }
            if (screen->window_mode_ && key == KEY_ESC) {
                screen->quit();
            } else {
                screen->input();
            }
        },
        .modifiers = [](void*, wl_keyboard*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) {},
        .repeat_info = [](void*, wl_keyboard*, int32_t, int32_t) {},
    };
    // The pointer. The enter does not end the screensaver: the surface comes
    // under the pointer where it stands. A motion does, but only one that
    // goes somewhere else than where the pointer entered.
    static const wl_pointer_listener pointer_listener = {
        .enter = [](void* data, wl_pointer* pointer, uint32_t serial, wl_surface*, wl_fixed_t x, wl_fixed_t y) {
            Seat* seat = static_cast<Seat*>(data);
            seat->x = x;
            seat->y = y;
            if (!seat->screen->window_mode_) {
                // No cursor surface: the cursor is hidden while it is on the surface.
                wl_pointer_set_cursor(pointer, serial, nullptr, 0, 0);
            }
        },
        .leave = [](void*, wl_pointer*, uint32_t, wl_surface*) {},
        .motion = [](void* data, wl_pointer*, uint32_t, wl_fixed_t x, wl_fixed_t y) {
            Seat* seat = static_cast<Seat*>(data);
            if (x != seat->x || y != seat->y) {
                seat->x = x;
                seat->y = y;
                seat->screen->input();
            }
        },
        .button = [](void* data, wl_pointer*, uint32_t, uint32_t, uint32_t, uint32_t state) {
            if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
                static_cast<Seat*>(data)->screen->input();
            }
        },
        .axis = [](void* data, wl_pointer*, uint32_t, uint32_t, wl_fixed_t) {
            static_cast<Seat*>(data)->screen->input();
        },
        .frame = [](void*, wl_pointer*) {},
        .axis_source = [](void*, wl_pointer*, uint32_t) {},
        .axis_stop = [](void*, wl_pointer*, uint32_t, uint32_t) {},
        .axis_discrete = [](void*, wl_pointer*, uint32_t, int32_t) {},
        .axis_value120 = [](void*, wl_pointer*, uint32_t, int32_t) {},
        .axis_relative_direction = [](void*, wl_pointer*, uint32_t, uint32_t) {},
    };
    // The touch screen: a touch ends the screensaver.
    static const wl_touch_listener touch_listener = {
        .down = [](void* data, wl_touch*, uint32_t, uint32_t, wl_surface*, int32_t, wl_fixed_t, wl_fixed_t) {
            static_cast<Seat*>(data)->screen->input();
        },
        .up = [](void*, wl_touch*, uint32_t, uint32_t, int32_t) {},
        .motion = [](void*, wl_touch*, uint32_t, int32_t, wl_fixed_t, wl_fixed_t) {},
        .frame = [](void*, wl_touch*) {},
        .cancel = [](void*, wl_touch*) {},
        .shape = [](void*, wl_touch*, int32_t, wl_fixed_t, wl_fixed_t) {},
        .orientation = [](void*, wl_touch*, int32_t, wl_fixed_t) {},
    };

    const bool has_keyboard = (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0;
    if (has_keyboard && keyboard == nullptr) {
        keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(keyboard, &keyboard_listener, this);
    } else if (!has_keyboard && keyboard != nullptr) {
        wl_keyboard_release(keyboard);
        keyboard = nullptr;
    }
    const bool has_pointer = (capabilities & WL_SEAT_CAPABILITY_POINTER) != 0;
    if (has_pointer && pointer == nullptr) {
        pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(pointer, &pointer_listener, this);
    } else if (!has_pointer && pointer != nullptr) {
        wl_pointer_release(pointer);
        pointer = nullptr;
    }
    const bool has_touch = (capabilities & WL_SEAT_CAPABILITY_TOUCH) != 0;
    if (has_touch && touch == nullptr) {
        touch = wl_seat_get_touch(seat);
        wl_touch_add_listener(touch, &touch_listener, this);
    } else if (!has_touch && touch != nullptr) {
        wl_touch_release(touch);
        touch = nullptr;
    }
}

Screen::Screen(bool window, int framerate)
    : window_mode_(window),
      frame_period_(framerate > 0 ? std::chrono::microseconds(1000000 / framerate) : std::chrono::microseconds(0))
{
    // SIGTERM ends the screensaver, as hd-kill-screensaver sends it, and
    // SIGINT, from a terminal. Blocked, they wait in the signalfd for run().
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGTERM);
    sigaddset(&signals, SIGINT);
    if (sigprocmask(SIG_BLOCK, &signals, nullptr) < 0) {
        fail(std::string("cannot block the signals: ") + std::strerror(errno));
    }
    signal_fd_ = signalfd(-1, &signals, SFD_CLOEXEC | SFD_NONBLOCK);
    if (signal_fd_ < 0) {
        fail(std::string("cannot create the signalfd: ") + std::strerror(errno));
    }

    display_ = wl_display_connect(nullptr);
    if (display_ == nullptr) {
        fail("cannot connect to the Wayland compositor");
    }

    // The registry lists the globals of the compositor, and later those that
    // come and go; bind the ones that the screensaver needs. The roundtrip
    // waits until they have all been sent.
    static const wl_registry_listener registry_listener = {
        .global = [](void* data, wl_registry*, uint32_t name, const char* interface, uint32_t version) {
            static_cast<Screen*>(data)->bind_global(name, interface, version);
        },
        .global_remove = [](void* data, wl_registry*, uint32_t name) {
            static_cast<Screen*>(data)->remove_global(name);
        },
    };
    registry_ = wl_display_get_registry(display_);
    wl_registry_add_listener(registry_, &registry_listener, this);
    wl_display_roundtrip(display_);
    if (compositor_ == nullptr) {
        fail("the compositor lacks wl_compositor 4");
    }
    if (!window_mode_ && layer_shell_ == nullptr) {
        fail("the compositor lacks zwlr_layer_shell_v1 4");
    }
    if (window_mode_ && wm_base_ == nullptr) {
        fail("the compositor lacks xdg_wm_base");
    }
    if (outputs_.empty()) {
        fail("the compositor has no outputs");
    }
    read_main_device();
}

void Screen::read_main_device()
{
    if (dmabuf_ == nullptr) {
        return;
    }
    // The default feedback names the main device, a dev_t, the GPU that
    // drives the compositor, and sends the table of the formats as a file,
    // which is not needed and is closed. The roundtrip waits for all of it;
    // after it the feedback and the dmabuf are not needed any more.
    static const zwp_linux_dmabuf_feedback_v1_listener feedback_listener = {
        .done = [](void*, zwp_linux_dmabuf_feedback_v1*) {},
        .format_table = [](void*, zwp_linux_dmabuf_feedback_v1*, int32_t fd, uint32_t) { close(fd); },
        .main_device = [](void* data, zwp_linux_dmabuf_feedback_v1*, wl_array* device) {
            if (device->size == sizeof(dev_t)) {
                dev_t dev;
                std::memcpy(&dev, device->data, sizeof(dev));
                static_cast<Screen*>(data)->main_device_ = DrmNode{major(dev), minor(dev)};
            }
        },
        .tranche_done = [](void*, zwp_linux_dmabuf_feedback_v1*) {},
        .tranche_target_device = [](void*, zwp_linux_dmabuf_feedback_v1*, wl_array*) {},
        .tranche_formats = [](void*, zwp_linux_dmabuf_feedback_v1*, wl_array*) {},
        .tranche_flags = [](void*, zwp_linux_dmabuf_feedback_v1*, uint32_t) {},
    };
    zwp_linux_dmabuf_feedback_v1* feedback = zwp_linux_dmabuf_v1_get_default_feedback(dmabuf_);
    zwp_linux_dmabuf_feedback_v1_add_listener(feedback, &feedback_listener, this);
    wl_display_roundtrip(display_);
    zwp_linux_dmabuf_feedback_v1_destroy(feedback);
    zwp_linux_dmabuf_v1_destroy(dmabuf_);
    dmabuf_ = nullptr;
}

Screen::~Screen()
{
    destroy_surfaces();
    seats_.clear();
    outputs_.clear();
    if (layer_shell_ != nullptr) {
        zwlr_layer_shell_v1_destroy(layer_shell_);
    }
    if (dmabuf_ != nullptr) {
        zwp_linux_dmabuf_v1_destroy(dmabuf_);
    }
    if (wm_base_ != nullptr) {
        xdg_wm_base_destroy(wm_base_);
    }
    if (compositor_ != nullptr) {
        wl_compositor_destroy(compositor_);
    }
    if (registry_ != nullptr) {
        wl_registry_destroy(registry_);
    }
    if (display_ != nullptr) {
        wl_display_flush(display_);
        wl_display_disconnect(display_);
    }
    if (signal_fd_ >= 0) {
        close(signal_fd_);
    }
}

void Screen::bind_global(uint32_t name, const char* interface, uint32_t version)
{
    const std::string iface = interface;
    if (iface == wl_compositor_interface.name && version >= 4) {
        compositor_ = static_cast<wl_compositor*>(wl_registry_bind(registry_, name, &wl_compositor_interface, 4));
    } else if (iface == zwlr_layer_shell_v1_interface.name && version >= 4 && !window_mode_) {
        layer_shell_ = static_cast<zwlr_layer_shell_v1*>(
            wl_registry_bind(registry_, name, &zwlr_layer_shell_v1_interface, 4));
    } else if (iface == zwp_linux_dmabuf_v1_interface.name && version >= 4 && dmabuf_ == nullptr &&
               gpu_ == nullptr) {
        // Only for the feedback at the start. Version 4 sends no events of
        // the dmabuf itself, which needs no listener.
        dmabuf_ = static_cast<zwp_linux_dmabuf_v1*>(
            wl_registry_bind(registry_, name, &zwp_linux_dmabuf_v1_interface, 4));
    } else if (iface == xdg_wm_base_interface.name && window_mode_) {
        static const xdg_wm_base_listener wm_base_listener = {
            .ping = [](void*, xdg_wm_base* wm_base, uint32_t serial) { xdg_wm_base_pong(wm_base, serial); },
        };
        wm_base_ = static_cast<xdg_wm_base*>(wl_registry_bind(registry_, name, &xdg_wm_base_interface, 1));
        xdg_wm_base_add_listener(wm_base_, &wm_base_listener, this);
    } else if (iface == wl_output_interface.name) {
        // Of the events of the output only the current mode is needed, for
        // its refresh rate: the layer surface gets the size of the output in
        // its configure.
        static const wl_output_listener output_listener = {
            .geometry = [](void*, wl_output*, int32_t, int32_t, int32_t, int32_t, int32_t, const char*, const char*,
                           int32_t) {},
            .mode = [](void* data, wl_output*, uint32_t flags, int32_t, int32_t, int32_t refresh) {
                if ((flags & WL_OUTPUT_MODE_CURRENT) != 0) {
                    static_cast<Output*>(data)->refresh = refresh;
                }
            },
            .done = [](void*, wl_output*) {},
            .scale = [](void*, wl_output*, int32_t) {},
            .name = [](void*, wl_output*, const char*) {},
            .description = [](void*, wl_output*, const char*) {},
        };
        auto output = std::make_unique<Output>();
        output->name = name;
        output->version = std::min(version, 4u);
        output->output = static_cast<wl_output*>(wl_registry_bind(registry_, name, &wl_output_interface,
                                                                  output->version));
        wl_output_add_listener(output->output, &output_listener, output.get());
        // An output that comes while run() goes on gets its surface at once.
        if (gpu_ != nullptr && !window_mode_) {
            output->surface = make_surface(output->output);
        }
        outputs_.push_back(std::move(output));
    } else if (iface == wl_seat_interface.name && version >= 5) {
        static const wl_seat_listener seat_listener = {
            .capabilities = [](void* data, wl_seat*, uint32_t capabilities) {
                static_cast<Seat*>(data)->set_capabilities(capabilities);
            },
            .name = [](void*, wl_seat*, const char*) {},
        };
        auto seat = std::make_unique<Seat>();
        seat->screen = this;
        seat->name = name;
        seat->seat = static_cast<wl_seat*>(wl_registry_bind(registry_, name, &wl_seat_interface, 5));
        wl_seat_add_listener(seat->seat, &seat_listener, seat.get());
        seats_.push_back(std::move(seat));
    }
}

void Screen::remove_global(uint32_t name)
{
    std::erase_if(outputs_, [name](const std::unique_ptr<Output>& o) { return o->name == name; });
    std::erase_if(seats_, [name](const std::unique_ptr<Seat>& s) { return s->name == name; });
}

std::unique_ptr<Screen::Surface> Screen::make_surface(wl_output* output)
{
    auto s = std::make_unique<Surface>();
    s->surface = wl_compositor_create_surface(compositor_);
    s->output = output;

    if (output != nullptr) {
        // The layer-shell role: on the overlay layer of the output, above
        // windows, bars and notifications, anchored to all four edges with
        // size 0 so that it gets the size of the whole output, ignoring the
        // exclusive zones of bars (-1), and with the keyboard to itself.
        static const zwlr_layer_surface_v1_listener layer_listener = {
            .configure = [](void* data, zwlr_layer_surface_v1* layer, uint32_t serial, uint32_t width,
                            uint32_t height) {
                zwlr_layer_surface_v1_ack_configure(layer, serial);
                auto* s = static_cast<Surface*>(data);
                s->pending_width = width;
                s->pending_height = height;
                s->due = true;
            },
            .closed = [](void* data, zwlr_layer_surface_v1*) { static_cast<Surface*>(data)->closed = true; },
        };
        s->layer = zwlr_layer_shell_v1_get_layer_surface(layer_shell_, s->surface, output,
                                                          ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, PROGRAM);
        zwlr_layer_surface_v1_add_listener(s->layer, &layer_listener, s.get());
        zwlr_layer_surface_v1_set_anchor(s->layer,
                                         ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
                                             ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
        zwlr_layer_surface_v1_set_size(s->layer, 0, 0);
        zwlr_layer_surface_v1_set_exclusive_zone(s->layer, -1);
        zwlr_layer_surface_v1_set_keyboard_interactivity(s->layer,
                                                         ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE);
    } else {
        // The window: the toplevel tells the size, the configure of the xdg
        // surface that comes after it makes it the size of the surface.
        static const xdg_surface_listener xdg_listener = {
            .configure = [](void* data, xdg_surface* xdg, uint32_t serial) {
                xdg_surface_ack_configure(xdg, serial);
                static_cast<Surface*>(data)->due = true;
            },
        };
        static const xdg_toplevel_listener toplevel_listener = {
            .configure = [](void* data, xdg_toplevel*, int32_t width, int32_t height, wl_array*) {
                auto* s = static_cast<Surface*>(data);
                s->pending_width = width > 0 ? static_cast<uint32_t>(width) : 0;
                s->pending_height = height > 0 ? static_cast<uint32_t>(height) : 0;
            },
            .close = [](void* data, xdg_toplevel*) { static_cast<Surface*>(data)->closed = true; },
            .configure_bounds = [](void*, xdg_toplevel*, int32_t, int32_t) {},
            .wm_capabilities = [](void*, xdg_toplevel*, wl_array*) {},
        };
        // The output the window is on, for the refresh rate: the last one it entered.
        static const wl_surface_listener surface_listener = {
            .enter = [](void* data, wl_surface*, wl_output* output) { static_cast<Surface*>(data)->output = output; },
            .leave = [](void*, wl_surface*, wl_output*) {},
            .preferred_buffer_scale = [](void*, wl_surface*, int32_t) {},
            .preferred_buffer_transform = [](void*, wl_surface*, uint32_t) {},
        };
        wl_surface_add_listener(s->surface, &surface_listener, s.get());
        s->xdg = xdg_wm_base_get_xdg_surface(wm_base_, s->surface);
        xdg_surface_add_listener(s->xdg, &xdg_listener, s.get());
        s->toplevel = xdg_surface_get_toplevel(s->xdg);
        xdg_toplevel_add_listener(s->toplevel, &toplevel_listener, s.get());
        xdg_toplevel_set_title(s->toplevel, PROGRAM);
        xdg_toplevel_set_app_id(s->toplevel, PROGRAM);
    }

    s->swapchain = std::make_unique<Swapchain>(*gpu_, display_, s->surface);
    s->scene = maker_->make_scene(*gpu_, s->swapchain->format());
    // The first commit, without a buffer, asks for the first configure.
    wl_surface_commit(s->surface);
    return s;
}

void Screen::configure(Surface& s, uint32_t width, uint32_t height)
{
    s.width = width;
    s.height = height;
    s.swapchain->resize(width, height);
}

void Screen::draw(Surface& s)
{
    // The size of the last configure; the window takes its own when the
    // compositor leaves it open.
    uint32_t width = s.pending_width;
    uint32_t height = s.pending_height;
    if (s.toplevel != nullptr) {
        if (width == 0) {
            width = s.width != 0 ? s.width : WINDOW_WIDTH;
        }
        if (height == 0) {
            height = s.height != 0 ? s.height : WINDOW_HEIGHT;
        }
    }
    configure(s, width, height);

    // The frame callback goes with the commit of the present, so it comes
    // when the compositor wants the frame after this one.
    static const wl_callback_listener frame_listener = {
        .done = [](void* data, wl_callback* callback, uint32_t) {
            auto* s = static_cast<Surface*>(data);
            wl_callback_destroy(callback);
            s->frame = nullptr;
            s->due = true;
        },
    };
    if (s.frame != nullptr) {
        wl_callback_destroy(s.frame);
    }
    s.frame = wl_surface_frame(s.surface);
    wl_callback_add_listener(s.frame, &frame_listener, &s);
    const int rate = refresh(s.output);
    const bool presented = s.swapchain->draw([&s, rate](VkCommandBuffer cmd, VkExtent2D extent) {
        s.scene->draw(cmd, extent, rate);
    });
    if (!presented) {
        // The request stays pending until the next commit; its callback would
        // come late. The frame is drawn again in a moment.
        wl_callback_destroy(s.frame);
        s.frame = nullptr;
        return;
    }
    s.due = false;
    // The turns stay one period apart, so the frames keep to the frame rate;
    // a surface that fell behind starts again from now.
    s.turn = std::max(s.turn + frame_period_, std::chrono::steady_clock::now());
}

int Screen::refresh(wl_output* output) const
{
    for (const auto& o : outputs_) {
        if (o->output == output) {
            return o->refresh;
        }
    }
    return 0;
}

void Screen::destroy_surfaces()
{
    window_.reset();
    for (auto& output : outputs_) {
        output->surface.reset();
    }
}

void Screen::input()
{
    if (!window_mode_) {
        quit();
    }
}

// The event loop: draw the frames that are due, then wait for events of the
// compositor and for the signals, and handle them.
void Screen::run(Gpu& gpu, SceneMaker& maker)
{
    gpu_ = &gpu;
    maker_ = &maker;
    // The surfaces hold resources of the device, which goes before the
    // screen: they go when run() ends, also on an error.
    struct Cleanup {
        Screen* screen;
        ~Cleanup() { screen->destroy_surfaces(); }
    } cleanup{this};

    if (window_mode_) {
        window_ = make_surface(nullptr);
    } else {
        for (auto& output : outputs_) {
            output->surface = make_surface(output->output);
        }
    }

    // wl_display_dispatch() would block on the connection alone; this waits
    // for the connection and the signals together, the way libwayland
    // describes: prepare the read, flush the requests, poll, then read and
    // dispatch the events.
    while (running_) {
        if (window_ != nullptr && window_->closed) {
            break;
        }
        for (auto& output : outputs_) {
            if (output->surface != nullptr && output->surface->closed) {
                output->surface.reset();
            }
        }

        bool retry = false;
        // The time until the first frame that waits for its turn; -1 when none does.
        int wait_ms = -1;
        const auto now = std::chrono::steady_clock::now();
        std::vector<Surface*> surfaces;
        if (window_ != nullptr) {
            surfaces.push_back(window_.get());
        }
        for (auto& output : outputs_) {
            if (output->surface != nullptr) {
                surfaces.push_back(output->surface.get());
            }
        }
        for (Surface* s : surfaces) {
            if (!s->due) {
                continue;
            }
            if (s->turn - FRAME_LEAD > now) {
                const auto wait = std::chrono::ceil<std::chrono::milliseconds>(s->turn - FRAME_LEAD - now);
                const int ms = static_cast<int>(wait.count());
                wait_ms = wait_ms < 0 ? ms : std::min(wait_ms, ms);
                continue;
            }
            draw(*s);
            retry = retry || s->due;
        }
        if (retry) {
            wait_ms = wait_ms < 0 ? RETRY_MS : std::min(wait_ms, RETRY_MS);
        }

        while (wl_display_prepare_read(display_) != 0) {
            if (wl_display_dispatch_pending(display_) < 0) {
                fail("lost the connection to the compositor");
            }
        }
        wl_display_flush(display_);
        pollfd fds[2] = {{wl_display_get_fd(display_), POLLIN, 0}, {signal_fd_, POLLIN, 0}};
        if (poll(fds, 2, wait_ms) < 0) {
            wl_display_cancel_read(display_);
            if (errno == EINTR) {
                continue;
            }
            fail(std::string("cannot wait for events: ") + std::strerror(errno));
        }
        if ((fds[0].revents & (POLLIN | POLLERR | POLLHUP)) != 0) {
            if (wl_display_read_events(display_) < 0) {
                fail("lost the connection to the compositor");
            }
        } else {
            wl_display_cancel_read(display_);
        }
        if (wl_display_dispatch_pending(display_) < 0) {
            fail("lost the connection to the compositor");
        }
        signalfd_siginfo info;
        if ((fds[1].revents & POLLIN) != 0 && read(signal_fd_, &info, sizeof info) == sizeof info) {
            quit();
        }
    }
}
