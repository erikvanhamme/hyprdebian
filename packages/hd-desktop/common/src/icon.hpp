/**
 * @file icon.hpp
 * @brief The icons of the options of hd-menu and the buttons of hd-message.
 */
#pragma once

#include <cairo.h>
#include <memory>
#include <string>

/** @brief The handle of an SVG document of librsvg, declared here so that only icon.cpp and draw.cpp include librsvg. */
typedef struct _RsvgHandle RsvgHandle;

/**
 * @brief An icon.
 *
 * A PNG, loaded into an image surface, or an SVG, which librsvg draws as
 * vectors at the size and scale of the frame. Both are null when there is
 * no icon.
 */
struct Icon {
    /** @brief The image of a PNG icon, or null. */
    std::shared_ptr<cairo_surface_t> png;
    /** @brief The document of an SVG icon, or null. */
    std::shared_ptr<RsvgHandle> svg;

    /**
     * @brief Tells whether there is an icon.
     * @return true when there is neither a PNG nor an SVG.
     */
    bool empty() const { return png == nullptr && svg == nullptr; }
};

/**
 * @brief Loads an icon.
 *
 * An SVG (.svg or .svgz) with librsvg, anything else as a PNG with cairo.
 *
 * @param path The path of the image.
 * @return The icon; empty, with a message on stderr, when it cannot be loaded.
 */
Icon load_icon(const std::string& path);
