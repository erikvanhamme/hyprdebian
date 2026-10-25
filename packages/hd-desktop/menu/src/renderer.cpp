#include "renderer.hpp"

#include "draw.hpp"

#include <algorithm>
#include <cmath>

namespace {

// The space between the border and the rows, so that the rounded selection
// sits inside the curve of the border.
const double PADDING = 6;
const double ROW_PAD_X = 8;
const double ROW_PAD_Y = 4;
const double ICON_GAP = 10;
const double BANNER_GAP = 8;
const double DIVIDER = 1;

} // namespace

void Renderer::draw(cairo_t* cr, int width, int height, Menu& menu) const
{
    const Theme& t = theme_;

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

    draw_box(cr, box, t);

    // What is inside the border has the inner radius, so that it follows
    // the curve of the border.
    const double inner_radius = std::max(0.0, t.radius - t.border);
    Rect content = inset(box, t.border + PADDING);

    // The banner at the top, with a divider under it.
    if (!menu.banner.empty()) {
        set_color(cr, t.light);
        draw_text(cr, menu.banner, {content.x, content.y, content.w - ROW_PAD_X, banner_h}, content.x + ROW_PAD_X);
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
        draw_text(cr, option.label, {r.x, r.y, r.w - ROW_PAD_X, r.h}, r.x + ROW_PAD_X + label_x);
    }
}
