/**
 * @file renderer.hpp
 * @brief How the message is drawn.
 *
 * cairo draws the shapes, the icons and the text into the pixels of a
 * buffer, and librsvg the SVG icons, with cairo. The renderer knows nothing
 * about Wayland: it gets a cairo context and the size of the surface in
 * logical pixels, and draws one complete frame.
 *
 * The surface covers the whole output. The message is a box with rounded
 * corners in the middle of it: the title at the top with a divider under it,
 * the text under that, wrapped to the width of the box, and the buttons in
 * a row at the bottom, all as wide as the widest. The box is as high as that
 * needs, at most as high as the output; text that does not fit is cut off.
 * The rest of the surface stays transparent. The colours, font, icon size,
 * border and radius come from the hd-theme.
 */
#pragma once

#include "message.hpp"
#include "theme.hpp"

#include <cairo.h>

/** @brief Draws frames of the message in a theme. */
class Renderer {
public:
    /**
     * @brief Makes a renderer for a theme.
     * @param theme The look of the message; the renderer keeps a copy.
     */
    explicit Renderer(const Theme& theme) : theme_(theme) {}

    /**
     * @brief Draws one complete frame.
     *
     * Clears the whole surface to transparent and draws the message in the
     * middle of it.
     *
     * @param cr      The cairo context of the buffer. The caller has scaled
     *                it for the buffer scale, so all coordinates are logical
     *                pixels.
     * @param width   The width of the surface, in logical pixels.
     * @param height  The height of the surface, in logical pixels.
     * @param message The message to draw.
     */
    void draw(cairo_t* cr, int width, int height, const Message& message) const;

private:
    /** @brief The look of the message. */
    Theme theme_;
};
