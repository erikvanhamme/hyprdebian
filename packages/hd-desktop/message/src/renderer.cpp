#include "renderer.hpp"

#include "draw.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {

// The space between the border and the content, so that the rounded buttons
// sit inside the curve of the border.
const double PADDING = 6;
const double ROW_PAD_X = 8;
const double ROW_PAD_Y = 4;
const double ICON_GAP = 10;
const double TITLE_GAP = 8;
const double DIVIDER = 1;
// The space between the text and the buttons, and between the buttons.
const double TEXT_GAP = 12;
const double BUTTON_GAP = 12;
// The space left and right of the content of a button.
const double BUTTON_PAD_X = 16;
// The width of the outline of a button that is not selected.
const double BUTTON_LINE = 2;
// Without a width in the message, the text wraps at this part of the output.
const double AUTO_WIDTH = 0.5;

double advance(cairo_t* cr, const std::string& text)
{
    cairo_text_extents_t extents;
    cairo_text_extents(cr, text.c_str(), &extents);
    return extents.x_advance;
}

// The length of the UTF-8 character at the start of text, in bytes: the
// first byte and the continuation bytes after it.
std::size_t char_length(const std::string& text, std::size_t start)
{
    std::size_t end = start + 1;
    while (end < text.size() && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) {
        ++end;
    }
    return end - start;
}

// Moves the longest start of word that fits in width, at least one
// character, from word to a line of its own in lines.
void break_word(cairo_t* cr, std::string& word, double width, std::vector<std::string>& lines)
{
    std::size_t fit = char_length(word, 0);
    while (fit < word.size()) {
        const std::size_t next = fit + char_length(word, fit);
        if (advance(cr, word.substr(0, next)) > width) {
            break;
        }
        fit = next;
    }
    lines.push_back(word.substr(0, fit));
    word.erase(0, fit);
}

// The text in lines no wider than width, in the current font: each line of
// text is wrapped at its spaces, and a word that is wider than width on its
// own is broken between characters.
std::vector<std::string> wrap(cairo_t* cr, const std::string& text, double width)
{
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (true) {
        const std::size_t newline = text.find('\n', start);
        const std::string paragraph = text.substr(start, newline == std::string::npos ? newline : newline - start);
        std::string line;
        std::size_t i = 0;
        while (i < paragraph.size()) {
            const std::size_t space = std::min(paragraph.find(' ', i), paragraph.size());
            std::string word = paragraph.substr(i, space - i);
            i = space + 1;
            if (word.empty()) {
                continue;
            }
            const std::string joined = line.empty() ? word : line + " " + word;
            if (advance(cr, joined) <= width) {
                line = joined;
                continue;
            }
            if (!line.empty()) {
                lines.push_back(line);
            }
            while (word.size() > char_length(word, 0) && advance(cr, word) > width) {
                break_word(cr, word, width, lines);
            }
            line = word;
        }
        lines.push_back(line);
        if (newline == std::string::npos) {
            return lines;
        }
        start = newline + 1;
    }
}

// The width of the widest line of the text, unwrapped.
double text_width(cairo_t* cr, const std::string& text)
{
    double w = 0;
    std::size_t start = 0;
    while (true) {
        const std::size_t newline = text.find('\n', start);
        w = std::max(w, advance(cr, text.substr(start, newline == std::string::npos ? newline : newline - start)));
        if (newline == std::string::npos) {
            return w;
        }
        start = newline + 1;
    }
}

void select_font(cairo_t* cr, const Theme& t, double size)
{
    cairo_select_font_face(cr, t.font.c_str(), CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
}

double line_height(cairo_t* cr)
{
    cairo_font_extents_t font;
    cairo_font_extents(cr, &font);
    return font.ascent + font.descent;
}

} // namespace

void Renderer::draw(cairo_t* cr, int width, int height, const Message& message) const
{
    const Theme& t = theme_;
    const double frame = 2 * (t.border + PADDING);

    // The title, in the big font.
    select_font(cr, t, t.font_size_big);
    const double title_h = message.title.empty() ? 0 : line_height(cr) + 2 * ROW_PAD_Y;
    const double title_w = message.title.empty() ? 0 : advance(cr, message.title) + 2 * ROW_PAD_X;

    // The text and the buttons, in the normal font. All buttons are as wide
    // as the widest, and as high as an icon when one has an icon.
    select_font(cr, t, t.font_size_normal);
    const double line_h = line_height(cr);
    const bool icons = std::any_of(message.buttons.begin(), message.buttons.end(),
                                   [](const Button& b) { return !b.icon.empty(); });
    double button_w = 0;
    for (const Button& button : message.buttons) {
        const double icon_w = button.icon.empty() ? 0 : t.icon_size + ICON_GAP;
        button_w = std::max(button_w, icon_w + advance(cr, button.label) + 2 * BUTTON_PAD_X);
    }
    const double button_h = std::max(line_h, icons ? t.icon_size : 0.0) + 2 * ROW_PAD_Y;
    const int count = static_cast<int>(message.buttons.size());
    const double buttons_w = count * button_w + (count - 1) * BUTTON_GAP;

    // The width of the box: the one the message sets, else what the title,
    // the buttons and the text need, with the text no wider than a part of
    // the output. In whole pixels so that the straight edges are sharp;
    // cairo antialiases the curves of the corners.
    double content_w = std::max(title_w, buttons_w);
    if (!message.text.empty()) {
        const double text_w = std::min(text_width(cr, message.text), width * AUTO_WIDTH - frame - 2 * ROW_PAD_X);
        content_w = std::max(content_w, text_w + 2 * ROW_PAD_X);
    }
    const double bw = resolve(message.width, width, content_w + frame);

    // The text wrapped to the width of the box, which then gives the height:
    // the title, the lines of text and the buttons under each other, with the
    // border and padding around.
    const double text_area_w = std::max(1.0, bw - frame - 2 * ROW_PAD_X);
    const std::vector<std::string> lines =
        message.text.empty() ? std::vector<std::string>() : wrap(cr, message.text, text_area_w);
    const double text_h = lines.empty() ? 0 : lines.size() * line_h + 2 * ROW_PAD_Y + TEXT_GAP;
    double content_h = text_h + button_h;
    if (!message.title.empty()) {
        content_h += title_h + TITLE_GAP;
    }
    const double bh = resolve(Size(), height, content_h + frame);
    const Rect box = {std::floor((width - bw) / 2), std::floor((height - bh) / 2), bw, bh};

    draw_box(cr, box, t);

    // What is inside the border has the inner radius, so that it follows
    // the curve of the border.
    const double inner_radius = std::max(0.0, t.radius - t.border);
    Rect content = inset(box, t.border + PADDING);
    cairo_save(cr);
    cairo_rectangle(cr, content.x, content.y, content.w, content.h);
    cairo_clip(cr);

    // The title at the top, with a divider under it.
    if (!message.title.empty()) {
        select_font(cr, t, t.font_size_big);
        set_color(cr, t.light);
        draw_text(cr, message.title, {content.x, content.y, content.w - ROW_PAD_X, title_h}, content.x + ROW_PAD_X);
        cairo_rectangle(cr, content.x, content.y + title_h + (TITLE_GAP - DIVIDER) / 2, content.w, DIVIDER);
        set_color(cr, t.normal);
        cairo_fill(cr);
        content.y += title_h + TITLE_GAP;
        content.h = std::max(0.0, content.h - title_h - TITLE_GAP);
        select_font(cr, t, t.font_size_normal);
    }

    // The buttons at the bottom, in the middle; the text fills what is above
    // them, and is cut off when it does not fit.
    const double buttons_y = content.y + std::max(0.0, content.h - button_h);
    set_color(cr, t.light);
    const Rect text_area = {content.x, content.y, content.w - ROW_PAD_X, std::max(0.0, buttons_y - content.y - TEXT_GAP)};
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const Rect line = {text_area.x, text_area.y + ROW_PAD_Y + i * line_h, text_area.w, line_h};
        if (line.y + line.h > text_area.y + text_area.h) {
            break;
        }
        draw_text(cr, lines[i], line, content.x + ROW_PAD_X);
    }

    double x = content.x + std::max(0.0, std::floor((content.w - buttons_w) / 2));
    for (int i = 0; i < count; ++i) {
        const Button& button = message.buttons[i];
        const Rect r = {x, buttons_y, button_w, button_h};
        // The selected button is filled, the others have an outline.
        if (i == message.selected) {
            rounded_rect(cr, r, inner_radius);
            set_color(cr, t.normal);
            cairo_fill(cr);
        } else {
            rounded_rect(cr, inset(r, BUTTON_LINE / 2), inner_radius - BUTTON_LINE / 2);
            set_color(cr, t.normal);
            cairo_set_line_width(cr, BUTTON_LINE);
            cairo_stroke(cr);
        }
        // The icon and the label, together in the middle of the button.
        const double icon_w = button.icon.empty() ? 0 : t.icon_size + ICON_GAP;
        const double inner_x = r.x + std::floor((r.w - icon_w - advance(cr, button.label)) / 2);
        if (!button.icon.empty()) {
            draw_icon(cr, button.icon, r, inner_x, t.icon_size);
        }
        set_color(cr, t.light);
        draw_text(cr, button.label, r, inner_x + icon_w);
        x += button_w + BUTTON_GAP;
    }
    cairo_restore(cr);
}
