#include "renderer.hpp"

#include <algorithm>
#include <cmath>
#include <librsvg/rsvg.h>

namespace {

struct Rect {
    double x, y, w, h;
};

// The space between the border and the rows, so that the rounded selection
// sits inside the curve of the border.
const double PADDING = 6;
const double ROW_PAD_X = 8;
const double ROW_PAD_Y = 4;
const double ICON_GAP = 10;
const double BANNER_GAP = 8;
const double DIVIDER = 1;

void set_color(cairo_t* cr, const Color& c, double alpha = 1.0)
{
    cairo_set_source_rgba(cr, c.r, c.g, c.b, alpha);
}

Rect inset(const Rect& r, double d)
{
    return {r.x + d, r.y + d, std::max(0.0, r.w - 2 * d), std::max(0.0, r.h - 2 * d)};
}

// The path of a rectangle with corners rounded with radius, clockwise from
// the top left: four quarter circles, joined by the straight edges.
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

// Draws one line of text in the current colour, from x and centred in area
// vertically. Text that does not fit is cut off at the edge of area.
void draw_text(cairo_t* cr, const std::string& text, const Rect& area, double x)
{
    cairo_font_extents_t font;
    cairo_font_extents(cr, &font);
    cairo_save(cr);
    cairo_rectangle(cr, area.x, area.y, area.w - ROW_PAD_X, area.h);
    cairo_clip(cr);
    // cairo places text by its baseline.
    cairo_move_to(cr, x, area.y + (area.h - font.ascent - font.descent) / 2 + font.ascent);
    // cairo finds the font with fontconfig and draws the glyphs with
    // FreeType, from the UTF-8 text.
    cairo_show_text(cr, text.c_str());
    cairo_restore(cr);
}

// Draws an icon scaled to fit a square of size at x, centred in area
// vertically.
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

// A size in whole logical pixels, no larger than the output: the size set in
// the menu, or fit, what the content needs.
double resolve(const Size& size, double output, double fit)
{
    double v = fit;
    if (size.set) {
        v = size.percent ? output * size.value / 100.0 : size.value;
    }
    return std::clamp(std::ceil(v), 0.0, output);
}

} // namespace

void Renderer::draw(cairo_t* cr, int width, int height, Menu& menu) const
{
    const Theme& t = theme_;

    // Start from fully transparent pixels: SOURCE replaces what is in the
    // buffer, where the default OVER would blend with the previous frame.
    // Everything outside the rounded box, the corners too, stays like this.
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    cairo_select_font_face(cr, t.font.c_str(), CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, t.font_size_big);
    cairo_font_extents_t font;
    cairo_font_extents(cr, &font);
    const double line_h = font.ascent + font.descent;

    // When an option has an icon, all labels move right to leave room for
    // a column of icons, and the rows are high enough for them.
    const bool icons = std::any_of(menu.options.begin(), menu.options.end(),
                                   [](const Option& o) { return !o.icon.empty(); });
    const double label_x = icons ? t.icon_size + ICON_GAP : 0;
    const double banner_h = line_h + 2 * ROW_PAD_Y;
    const double row_h = std::max(line_h, icons ? t.icon_size : 0.0) + 2 * ROW_PAD_Y;
    const int count = static_cast<int>(menu.options.size());

    // The size the content needs, for a width or height that the menu does
    // not set: the widest line of text, and the banner and all options under
    // each other, with the border and padding around. In whole pixels so
    // that the straight edges are sharp; cairo antialiases the curves of the
    // corners.
    const double frame = 2 * (t.border + PADDING);
    double content_w = 0;
    double content_h = count * row_h;
    cairo_text_extents_t text;
    if (!menu.banner.empty()) {
        cairo_text_extents(cr, menu.banner.c_str(), &text);
        content_w = text.x_advance;
        content_h += banner_h + BANNER_GAP;
    }
    for (const Option& option : menu.options) {
        cairo_text_extents(cr, option.label.c_str(), &text);
        content_w = std::max(content_w, label_x + text.x_advance);
    }
    const double bw = resolve(menu.width, width, content_w + 2 * ROW_PAD_X + frame);
    const double bh = resolve(menu.height, height, content_h + frame);
    const Rect box = {std::floor((width - bw) / 2), std::floor((height - bh) / 2), bw, bh};

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

    // What is inside the border has the inner radius, so that it follows
    // the curve of the border.
    const double inner_radius = std::max(0.0, t.radius - t.border);
    Rect content = inset(box, t.border + PADDING);

    // The banner at the top, with a divider under it.
    if (!menu.banner.empty()) {
        set_color(cr, t.light);
        draw_text(cr, menu.banner, {content.x, content.y, content.w, banner_h}, content.x + ROW_PAD_X);
        cairo_rectangle(cr, content.x, content.y + banner_h + (BANNER_GAP - DIVIDER) / 2, content.w, DIVIDER);
        set_color(cr, t.normal);
        cairo_fill(cr);
        content.y += banner_h + BANNER_GAP;
        content.h = std::max(0.0, content.h - banner_h - BANNER_GAP);
    }

    // The options that fit, scrolled so that the selected one is among them.
    rows_ = std::max(1, static_cast<int>(content.h / row_h));
    menu.scroll = std::clamp(menu.scroll, menu.selected - rows_ + 1, menu.selected);
    for (int i = 0; i < rows_ && menu.scroll + i < count; ++i) {
        const int row = menu.scroll + i;
        const Option& option = menu.options[row];
        const Rect r = {content.x, content.y + i * row_h, content.w, row_h};
        if (row == menu.selected) {
            rounded_rect(cr, r, inner_radius);
            set_color(cr, t.normal);
            cairo_fill(cr);
        }
        if (!option.icon.empty()) {
            draw_icon(cr, option.icon, r, r.x + ROW_PAD_X, t.icon_size);
        }
        set_color(cr, t.light);
        draw_text(cr, option.label, r, r.x + ROW_PAD_X + label_x);
    }
}
