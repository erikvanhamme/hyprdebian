#include "window.hpp"

#include "program.hpp"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstring>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <wayland-client.h>

// The header is C, with an argument named namespace, a keyword in C++. It
// comes after the other headers, so that the define touches only it.
#define namespace namespace_
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#undef namespace

namespace {

void fail(const std::string& what)
{
    throw std::runtime_error(what);
}

} // namespace

// libwayland calls C function pointers, with the pointer given to
// add_listener as data. The listeners here are captureless lambdas, which
// convert to function pointers; defined in member functions, they may call
// the private members of Window.

Window::Window(WindowHandler& handler) : handler_(handler)
{
    display_ = wl_display_connect(nullptr);
    if (display_ == nullptr) {
        fail("cannot connect to the Wayland compositor");
    }

    // The registry lists the globals of the compositor; bind the ones that
    // the program needs. The roundtrip waits until they have all been sent.
    static const wl_registry_listener registry_listener = {
        .global = [](void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
            static_cast<Window*>(data)->bind_global(registry, name, interface, version);
        },
        .global_remove = [](void*, wl_registry*, uint32_t) {},
    };
    registry_ = wl_display_get_registry(display_);
    wl_registry_add_listener(registry_, &registry_listener, this);
    wl_display_roundtrip(display_);
    if (compositor_ == nullptr || shm_ == nullptr || seat_ == nullptr || layer_shell_ == nullptr) {
        fail("the compositor lacks wl_compositor 4, wl_shm, wl_seat 5 or zwlr_layer_shell_v1");
    }

    // The keyboard. The arrows, Enter and Escape go by key code, which needs
    // no keymap. Letters such as q go by keysym, from the keymap that the
    // compositor sends, so that they follow the keyboard layout.
    // A held key repeats at the rate and after the delay of the compositor,
    // those of input:repeat_rate and input:repeat_delay in Hyprland, with a
    // timer that run() polls. A second timer is the one of start_timer().
    xkb_context_ = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (xkb_context_ == nullptr) {
        fail("cannot create the xkb context");
    }
    repeat_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (repeat_fd_ < 0) {
        fail(std::string("cannot create the key repeat timer: ") + std::strerror(errno));
    }
    timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (timer_fd_ < 0) {
        fail(std::string("cannot create the timer: ") + std::strerror(errno));
    }
    static const wl_keyboard_listener keyboard_listener = {
        .keymap = [](void* data, wl_keyboard*, uint32_t format, int32_t fd, uint32_t size) {
            static_cast<Window*>(data)->set_keymap(format, fd, size);
        },
        .enter = [](void*, wl_keyboard*, uint32_t, wl_surface*, wl_array*) {},
        .leave = [](void* data, wl_keyboard*, uint32_t, wl_surface*) { static_cast<Window*>(data)->stop_repeat(); },
        .key = [](void* data, wl_keyboard*, uint32_t, uint32_t, uint32_t key, uint32_t state) {
            static_cast<Window*>(data)->key_event(key, state == WL_KEYBOARD_KEY_STATE_PRESSED);
        },
        .modifiers = [](void* data, wl_keyboard*, uint32_t, uint32_t depressed, uint32_t latched, uint32_t locked,
                        uint32_t group) {
            Window* window = static_cast<Window*>(data);
            if (window->xkb_state_ != nullptr) {
                xkb_state_update_mask(window->xkb_state_, depressed, latched, locked, 0, 0, group);
            }
        },
        .repeat_info = [](void* data, wl_keyboard*, int32_t rate, int32_t delay) {
            static_cast<Window*>(data)->set_repeat(rate, delay);
        },
    };
    keyboard_ = wl_seat_get_keyboard(seat_);
    wl_keyboard_add_listener(keyboard_, &keyboard_listener, this);

    // The surface. wl_surface 6 tells the scale to draw at for the outputs
    // it is on.
    static const wl_surface_listener surface_listener = {
        .enter = [](void*, wl_surface*, wl_output*) {},
        .leave = [](void*, wl_surface*, wl_output*) {},
        .preferred_buffer_scale = [](void* data, wl_surface*, int32_t factor) {
            static_cast<Window*>(data)->set_scale(factor);
        },
        .preferred_buffer_transform = [](void*, wl_surface*, uint32_t) {},
    };
    surface_ = wl_compositor_create_surface(compositor_);
    wl_surface_add_listener(surface_, &surface_listener, this);

    // The layer-shell role: on the overlay layer, above windows and bars,
    // anchored to all four edges with size 0 so that it gets the size of
    // the whole output, ignoring the exclusive zones of bars (-1), and with
    // the keyboard to itself while it is open. No output: the compositor
    // picks one, Hyprland the one with the focus.
    static const zwlr_layer_surface_v1_listener layer_surface_listener = {
        .configure = [](void* data, zwlr_layer_surface_v1*, uint32_t serial, uint32_t width, uint32_t height) {
            static_cast<Window*>(data)->configure(serial, width, height);
        },
        .closed = [](void* data, zwlr_layer_surface_v1*) { static_cast<Window*>(data)->handler_.closed(); },
    };
    layer_surface_ = zwlr_layer_shell_v1_get_layer_surface(layer_shell_, surface_, nullptr,
                                                           ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, PROGRAM);
    zwlr_layer_surface_v1_add_listener(layer_surface_, &layer_surface_listener, this);
    zwlr_layer_surface_v1_set_anchor(layer_surface_,
                                     ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
                                         ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    zwlr_layer_surface_v1_set_size(layer_surface_, 0, 0);
    zwlr_layer_surface_v1_set_exclusive_zone(layer_surface_, -1);
    zwlr_layer_surface_v1_set_keyboard_interactivity(layer_surface_,
                                                     ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE);
    // The first commit, without a buffer, asks for the first configure. It
    // arrives in run(); from then on the handler gets events.
    wl_surface_commit(surface_);
}

Window::~Window()
{
    destroy_buffers();
    if (layer_surface_ != nullptr) {
        zwlr_layer_surface_v1_destroy(layer_surface_);
    }
    if (surface_ != nullptr) {
        wl_surface_destroy(surface_);
    }
    if (keyboard_ != nullptr) {
        wl_keyboard_release(keyboard_);
    }
    if (xkb_state_ != nullptr) {
        xkb_state_unref(xkb_state_);
    }
    if (xkb_keymap_ != nullptr) {
        xkb_keymap_unref(xkb_keymap_);
    }
    if (xkb_context_ != nullptr) {
        xkb_context_unref(xkb_context_);
    }
    if (repeat_fd_ >= 0) {
        close(repeat_fd_);
    }
    if (timer_fd_ >= 0) {
        close(timer_fd_);
    }
    if (layer_shell_ != nullptr) {
        zwlr_layer_shell_v1_destroy(layer_shell_);
    }
    if (seat_ != nullptr) {
        wl_seat_release(seat_);
    }
    if (shm_ != nullptr) {
        wl_shm_destroy(shm_);
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
}

void Window::bind_global(wl_registry* registry, uint32_t name, const char* interface, uint32_t version)
{
    const std::string iface = interface;
    if (iface == wl_compositor_interface.name && version >= 4) {
        compositor_ = static_cast<wl_compositor*>(
            wl_registry_bind(registry, name, &wl_compositor_interface, std::min(version, 6u)));
    } else if (iface == wl_shm_interface.name) {
        shm_ = static_cast<wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
    } else if (iface == wl_seat_interface.name && seat_ == nullptr && version >= 5) {
        // The keyboard comes from the seat; the program assumes it has one.
        seat_ = static_cast<wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, 5));
    } else if (iface == zwlr_layer_shell_v1_interface.name && version >= 4) {
        layer_shell_ = static_cast<zwlr_layer_shell_v1*>(
            wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 4));
    }
}

void Window::set_keymap(uint32_t format, int fd, uint32_t size)
{
    // The keymap is a string ending in a NUL, in memory to map read only.
    xkb_keymap* keymap = nullptr;
    if (format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 && size > 0) {
        void* map = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (map != MAP_FAILED) {
            keymap = xkb_keymap_new_from_string(xkb_context_, static_cast<const char*>(map),
                                                XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
            munmap(map, size);
        }
    }
    close(fd);
    if (keymap == nullptr) {
        return;
    }
    xkb_state* state = xkb_state_new(keymap);
    if (state == nullptr) {
        xkb_keymap_unref(keymap);
        return;
    }
    if (xkb_state_ != nullptr) {
        xkb_state_unref(xkb_state_);
    }
    if (xkb_keymap_ != nullptr) {
        xkb_keymap_unref(xkb_keymap_);
    }
    xkb_keymap_ = keymap;
    xkb_state_ = state;
}

void Window::key_event(uint32_t key, bool pressed)
{
    if (!pressed) {
        if (key == repeat_key_) {
            stop_repeat();
        }
        return;
    }
    stop_repeat();
    // Keys that do not repeat in the keymap, such as Shift, stay single;
    // without a keymap every key repeats.
    bool repeats = repeat_rate_ > 0;
    if (repeats && xkb_keymap_ != nullptr) {
        repeats = xkb_keymap_key_repeats(xkb_keymap_, key + 8) != 0;
    }
    if (repeats) {
        const long interval = 1000000000L / repeat_rate_;
        itimerspec spec = {};
        spec.it_value.tv_sec = repeat_delay_ / 1000;
        spec.it_value.tv_nsec = (repeat_delay_ % 1000) * 1000000L;
        spec.it_interval.tv_sec = interval / 1000000000L;
        spec.it_interval.tv_nsec = interval % 1000000000L;
        // A delay of 0 would disarm the timer; the first repeat comes at once instead.
        if (spec.it_value.tv_sec == 0 && spec.it_value.tv_nsec == 0) {
            spec.it_value.tv_nsec = 1;
        }
        timerfd_settime(repeat_fd_, 0, &spec, nullptr);
        repeat_key_ = key;
    }
    handler_.key(key, keysym(key));
}

void Window::set_repeat(int32_t rate, int32_t delay)
{
    repeat_rate_ = std::max(rate, 0);
    repeat_delay_ = std::max(delay, 0);
    stop_repeat();
}

void Window::stop_repeat()
{
    const itimerspec off = {};
    timerfd_settime(repeat_fd_, 0, &off, nullptr);
    repeat_key_ = 0;
}

void Window::repeat()
{
    uint64_t expirations = 0;
    if (read(repeat_fd_, &expirations, sizeof expirations) != sizeof expirations) {
        return;
    }
    // When the program falls behind, the repeats it missed still count, so that
    // the selection moves as far as the time held asks.
    for (uint64_t i = 0; i < expirations && running_ && repeat_key_ != 0; ++i) {
        handler_.key(repeat_key_, keysym(repeat_key_));
    }
}

void Window::start_timer(int ms)
{
    itimerspec spec = {};
    // A time of 0 would disarm the timer.
    ms = std::max(ms, 1);
    spec.it_value.tv_sec = ms / 1000;
    spec.it_value.tv_nsec = (ms % 1000) * 1000000L;
    timerfd_settime(timer_fd_, 0, &spec, nullptr);
}

void Window::stop_timer()
{
    const itimerspec off = {};
    timerfd_settime(timer_fd_, 0, &off, nullptr);
}

xkb_keysym_t Window::keysym(uint32_t key) const
{
    if (xkb_state_ == nullptr) {
        return XKB_KEY_NoSymbol;
    }
    // xkb numbers the keys 8 higher than the Linux key codes.
    return xkb_state_key_get_one_sym(xkb_state_, key + 8);
}

void Window::configure(uint32_t serial, uint32_t width, uint32_t height)
{
    zwlr_layer_surface_v1_ack_configure(layer_surface_, serial);
    if (static_cast<int>(width) != width_ || static_cast<int>(height) != height_) {
        width_ = static_cast<int>(width);
        height_ = static_cast<int>(height);
        destroy_buffers();
    }
    configured_ = true;
    redraw();
}

void Window::set_scale(int scale)
{
    if (scale < 1 || scale == scale_) {
        return;
    }
    scale_ = scale;
    destroy_buffers();
    redraw();
}

void Window::redraw()
{
    dirty_ = true;
    draw_if_possible();
}

// Draws when there is something new to draw and a buffer is free; otherwise
// the release of a buffer calls this again.
void Window::draw_if_possible()
{
    if (!configured_ || !dirty_ || width_ <= 0 || height_ <= 0) {
        return;
    }
    Buffer* b = free_buffer();
    if (b == nullptr) {
        return;
    }

    // cairo draws straight into the shared memory. Scaling the context lets
    // the renderer work in logical pixels on a buffer of scale times as many
    // pixels each way.
    cairo_t* cr = cairo_create(b->cairo);
    cairo_scale(cr, scale_, scale_);
    handler_.draw(cr, width_, height_);
    cairo_destroy(cr);
    cairo_surface_flush(b->cairo);

    wl_surface_set_buffer_scale(surface_, scale_);
    wl_surface_attach(surface_, b->buffer, 0, 0);
    wl_surface_damage_buffer(surface_, 0, 0, INT32_MAX, INT32_MAX);
    // Everything above is pending state; the commit applies it at once.
    wl_surface_commit(surface_);
    b->busy = true;
    dirty_ = false;
}

Window::Buffer* Window::free_buffer()
{
    for (Buffer& b : buffers_) {
        if (b.buffer == nullptr) {
            create_buffer(b);
        }
        if (!b.busy) {
            return &b;
        }
    }
    return nullptr;
}

void Window::create_buffer(Buffer& b)
{
    const int w = width_ * scale_;
    const int h = height_ * scale_;
    // cairo's ARGB32 is a 32 bit word per pixel in native byte order, alpha
    // premultiplied, the same as wl_shm's ARGB8888 on little endian.
    const int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, w);
    b.size = static_cast<std::size_t>(stride) * h;

    // Anonymous memory with a file descriptor, which the compositor maps too.
    const int fd = memfd_create(PROGRAM, MFD_CLOEXEC);
    if (fd < 0 || ftruncate(fd, static_cast<off_t>(b.size)) < 0) {
        fail(std::string("cannot allocate a buffer: ") + std::strerror(errno));
    }
    b.data = mmap(nullptr, b.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (b.data == MAP_FAILED) {
        close(fd);
        fail(std::string("cannot map a buffer: ") + std::strerror(errno));
    }
    wl_shm_pool* pool = wl_shm_create_pool(shm_, fd, static_cast<int32_t>(b.size));
    b.buffer = wl_shm_pool_create_buffer(pool, 0, w, h, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);

    static const wl_buffer_listener buffer_listener = {
        .release = [](void* data, wl_buffer*) {
            auto* buf = static_cast<Buffer*>(data);
            buf->busy = false;
            buf->window->draw_if_possible();
        },
    };
    b.window = this;
    b.busy = false;
    wl_buffer_add_listener(b.buffer, &buffer_listener, &b);
    b.cairo = cairo_image_surface_create_for_data(static_cast<unsigned char*>(b.data), CAIRO_FORMAT_ARGB32, w, h,
                                                  stride);
}

// For a new size or scale. The compositor keeps what it shows of a buffer
// that is destroyed while it is busy.
void Window::destroy_buffers()
{
    for (Buffer& b : buffers_) {
        if (b.cairo != nullptr) {
            cairo_surface_destroy(b.cairo);
        }
        if (b.buffer != nullptr) {
            wl_buffer_destroy(b.buffer);
        }
        if (b.data != nullptr) {
            munmap(b.data, b.size);
        }
        b = Buffer();
    }
}

// The event loop: wait for events of the compositor and handle them. The
// handlers draw and send requests; wl_display_dispatch flushes them before
// it waits.
void Window::run()
{
    // wl_display_dispatch() would block on the connection alone; this waits
    // for the connection and the two timers together, the way libwayland
    // describes: prepare the read, flush the requests, poll, then read and
    // dispatch the events.
    while (running_) {
        while (wl_display_prepare_read(display_) != 0) {
            if (wl_display_dispatch_pending(display_) < 0) {
                fail("lost the connection to the compositor");
            }
        }
        wl_display_flush(display_);
        pollfd fds[3] = {{wl_display_get_fd(display_), POLLIN, 0}, {repeat_fd_, POLLIN, 0}, {timer_fd_, POLLIN, 0}};
        if (poll(fds, 3, -1) < 0) {
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
        if (running_ && (fds[1].revents & POLLIN) != 0) {
            repeat();
        }
        // A key handled above may have started the timer again; then the read
        // finds nothing, and the timer goes on.
        uint64_t expirations = 0;
        if (running_ && (fds[2].revents & POLLIN) != 0 &&
            read(timer_fd_, &expirations, sizeof expirations) == sizeof expirations) {
            handler_.timeout();
        }
    }
}
