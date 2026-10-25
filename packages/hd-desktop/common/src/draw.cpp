#include "draw.hpp"

#include <algorithm>
#include <cmath>
#include <librsvg/rsvg.h>

void set_color(cairo_t* cr, const Color& c, double alpha)
{
    cairo_set_source_rgba(cr, c.r, c.g, c.b, alpha);
}

Rect inset(const Rect& r, double d)
{
    return {r.x + d, r.y + d, std::max(0.0, r.w - 2 * d), std::max(0.0, r.h - 2 * d)};
}

// Four quarter circles, clockwise from the top left, joined by the straight
// edges.
void rounded_rect(cairo_t* cr, const Rect& r, double radius)
{
    radius = std::clamp(radius, 0.0, std::min(r.w, r.h) / 2);
    cairo_new_sub_path(cr);
    cairo_arc(cr, r.x + r.w - radius, r.y + radius, radius, -M_PI / 2, 0);
    cairo_arc(cr, r.x + r.w - radius, r.y + r.h - radius, radius, 0, M_PI / 2);
    cairo_arc(cr, r.x + radius, r.y + r.h - radius, radius, M_PI / 2, M_PI);
    cairo_arc(cr, r.x + radius, r.y + radius, radius, M_PI, 3 * M_PI / 2);
    cairo_close_path(cr);
}

void draw_box(cairo_t* cr, const Rect& box, const Theme& t)
{
    // Start from fully transparent pixels: SOURCE replaces what is in the
    // buffer, where the default OVER would blend with the previous frame.
    // Everything outside the rounded box, the corners too, stays like this.
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    // The background, translucent: opacity applies to it alone.
    rounded_rect(cr, box, t.radius);
    set_color(cr, t.dark, t.opacity);
    cairo_fill(cr);

    // The outline of the focused window. cairo strokes centred on the path,
    // so the path runs half the border inside the edge, with a radius that
    // is as much smaller: the outer edge of the stroke then follows the
    // curve of the box, and the inner edge has the inner radius.
    if (t.border > 0) {
        rounded_rect(cr, inset(box, t.border / 2), t.radius - t.border / 2);
        set_color(cr, t.highlight);
        cairo_set_line_width(cr, t.border);
        cairo_stroke(cr);
    }
}

void draw_text(cairo_t* cr, const std::string& text, const Rect& area, double x)
{
    cairo_font_extents_t font;
    cairo_font_extents(cr, &font);
    cairo_save(cr);
    cairo_rectangle(cr, area.x, area.y, area.w, area.h);
    cairo_clip(cr);
    // cairo places text by its baseline.
    cairo_move_to(cr, x, area.y + (area.h - font.ascent - font.descent) / 2 + font.ascent);
    // cairo finds the font with fontconfig and draws the glyphs with
    // FreeType, from the UTF-8 text.
    cairo_show_text(cr, text.c_str());
    cairo_restore(cr);
}

void draw_icon(cairo_t* cr, const Icon& icon, const Rect& area, double x, double size)
{
    const double y = area.y + (area.h - size) / 2;
    if (icon.svg != nullptr) {
        // librsvg draws the vectors of the SVG with cairo, fitted into the
        // square with the aspect ratio of the document kept, at whatever
        // scale cr has: sharp at any buffer scale.
        const RsvgRectangle viewport = {x, y, size, size};
        rsvg_handle_render_document(icon.svg.get(), cr, &viewport, nullptr);
        return;
    }
    cairo_surface_t* image = icon.png.get();
    const int w = cairo_image_surface_get_width(image);
    const int h = cairo_image_surface_get_height(image);
    const double scale = size / std::max(w, h);
    cairo_save(cr);
    cairo_translate(cr, x + (size - w * scale) / 2, y + (size - h * scale) / 2);
    cairo_scale(cr, scale, scale);
    cairo_set_source_surface(cr, image, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
    cairo_paint(cr);
    cairo_restore(cr);
}
