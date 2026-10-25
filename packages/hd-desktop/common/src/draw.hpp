/**
 * @file draw.hpp
 * @brief The drawing that hd-menu and hd-message share, with cairo.
 *
 * All coordinates are logical pixels.
 */
#pragma once

#include "icon.hpp"
#include "theme.hpp"

#include <cairo.h>
#include <string>

/** @brief A rectangle. */
struct Rect {
    /** @brief The left edge. */
    double x;
    /** @brief The top edge. */
    double y;
    /** @brief The width. */
    double w;
    /** @brief The height. */
    double h;
};

/**
 * @brief Makes a colour the source of cr.
 * @param cr    The cairo context.
 * @param c     The colour.
 * @param alpha The opacity, from 0 to 1.
 */
void set_color(cairo_t* cr, const Color& c, double alpha = 1.0);

/**
 * @brief A rectangle made smaller on all sides.
 * @param r The rectangle.
 * @param d How much smaller on each side.
 * @return The smaller rectangle, never of a negative size.
 */
Rect inset(const Rect& r, double d);

/**
 * @brief Adds the path of a rectangle with rounded corners to cr.
 * @param cr     The cairo context.
 * @param r      The rectangle.
 * @param radius The radius of the corners, at most half the shorter side.
 */
void rounded_rect(cairo_t* cr, const Rect& r, double radius);

/**
 * @brief Draws the box of a window: the background and the border.
 *
 * Clears the whole surface to transparent first. The background is dark,
 * with the opacity of the theme, the border is highlight.
 *
 * @param cr  The cairo context.
 * @param box The box, in the middle of the surface.
 * @param t   The theme.
 */
void draw_box(cairo_t* cr, const Rect& box, const Theme& t);

/**
 * @brief Draws one line of text in the current colour and font.
 * @param cr   The cairo context.
 * @param text The text, in UTF-8.
 * @param area The text is centred in it vertically, and cut off at its edges.
 * @param x    Where the text starts.
 */
void draw_text(cairo_t* cr, const std::string& text, const Rect& area, double x);

/**
 * @brief Draws an icon, scaled to fit a square, with its aspect ratio kept.
 * @param cr   The cairo context.
 * @param icon The icon; not empty.
 * @param area The square is centred in it vertically.
 * @param x    The left edge of the square.
 * @param size The width and height of the square.
 */
void draw_icon(cairo_t* cr, const Icon& icon, const Rect& area, double x, double size);
