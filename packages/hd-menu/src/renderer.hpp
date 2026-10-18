/**
 * @file renderer.hpp
 * @brief How the menu is drawn.
 *
 * cairo draws the shapes, the icons and the text into the pixels of a
 * buffer, and librsvg the SVG icons, with cairo. The renderer knows nothing
 * about Wayland: it gets a cairo context and the size of the surface in
 * logical pixels, and draws one complete frame.
 *
 * The surface covers the whole output. The menu is a box with rounded
 * corners in the middle of it, of the size the menu sets or just large
 * enough for the banner and the options; the rest stays transparent. The
 * colours, font, icon size, border and radius come from the hd-theme.
 */
#pragma once

#include "menu.hpp"
#include "theme.hpp"

#include <cairo.h>

/** @brief Draws frames of the menu in a theme. */
class Renderer {
public:
    /**
     * @brief Makes a renderer for a theme.
     * @param theme The look of the menu; the renderer keeps a copy.
     */
    explicit Renderer(const Theme& theme) : theme_(theme) {}

    /**
     * @brief Draws one complete frame.
     *
     * Clears the whole surface to transparent and draws the menu in the
     * middle of it. Scrolls menu so that the selected option is shown.
     *
     * @param cr     The cairo context of the buffer. The caller has scaled
     *               it for the buffer scale, so all coordinates are logical
     *               pixels.
     * @param width  The width of the surface, in logical pixels.
     * @param height The height of the surface, in logical pixels.
     * @param menu   The menu to draw; its scroll may change.
     */
    void draw(cairo_t* cr, int width, int height, Menu& menu) const;

    /**
     * @brief The number of options that fit in the menu, for Page Up and Page Down.
     * @return The number of rows of the last frame drawn, at least 1.
     */
    int rows() const { return rows_; }

private:
    /** @brief The look of the menu. */
    Theme theme_;
    /** @brief The number of options that fit, as of the last frame. */
    mutable int rows_ = 1;
};
