#include "icon.hpp"

#include "program.hpp"

#include <iostream>
#include <librsvg/rsvg.h>

namespace {

bool ends_with(const std::string& s, const std::string& end)
{
    return s.size() >= end.size() && s.compare(s.size() - end.size(), end.size(), end) == 0;
}

} // namespace

// Loads an SVG with librsvg, a PNG with cairo, which reads PNG itself.
Icon load_icon(const std::string& path)
{
    Icon icon;
    if (ends_with(path, ".svg") || ends_with(path, ".svgz")) {
        GError* error = nullptr;
        RsvgHandle* handle = rsvg_handle_new_from_file(path.c_str(), &error);
        if (handle == nullptr) {
            std::cerr << PROGRAM << ": cannot load the icon " << path << ": " << error->message << "\n";
            g_error_free(error);
            return icon;
        }
        icon.svg = std::shared_ptr<RsvgHandle>(handle, g_object_unref);
        return icon;
    }
    cairo_surface_t* image = cairo_image_surface_create_from_png(path.c_str());
    if (cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {
        std::cerr << PROGRAM << ": cannot load the icon " << path << ": "
                  << cairo_status_to_string(cairo_surface_status(image)) << "\n";
        cairo_surface_destroy(image);
        return icon;
    }
    icon.png = std::shared_ptr<cairo_surface_t>(image, cairo_surface_destroy);
    return icon;
}
