#include "window.hpp"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
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
    // hd-menu needs. The roundtrip waits until they have all been sent.
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

    // The keyboard. Only the key codes matter: hd-menu needs no keymap for
    // the arrows, Enter and Escape. The keymap still comes as a file
    // descriptor, which has to be closed.
    static const wl_keyboard_listener keyboard_listener = {
        .keymap = [](void*, wl_keyboard*, uint32_t, int32_t fd, uint32_t) { close(fd); },
        .enter = [](void*, wl_keyboard*, uint32_t, wl_surface*, wl_array*) {},
        .leave = [](void*, wl_keyboard*, uint32_t, wl_surface*) {},
        .key = [](void* data, wl_keyboard*, uint32_t, uint32_t, uint32_t key, uint32_t state) {
            if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
                static_cast<Window*>(data)->handler_.key(key);
            }
        },
        .modifiers = [](void*, wl_keyboard*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) {},
        .repeat_info = [](void*, wl_keyboard*, int32_t, int32_t) {},
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
                                                           ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "hd-menu");
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
        // The keyboard comes from the seat; hd-menu assumes it has one.
        seat_ = static_cast<wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, 5));
    } else if (iface == zwlr_layer_shell_v1_interface.name && version >= 4) {
        layer_shell_ = static_cast<zwlr_layer_shell_v1*>(
            wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 4));
    }
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
    const int fd = memfd_create("hd-menu", MFD_CLOEXEC);
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
    while (running_) {
        if (wl_display_dispatch(display_) < 0) {
            fail("lost the connection to the compositor");
        }
    }
}
