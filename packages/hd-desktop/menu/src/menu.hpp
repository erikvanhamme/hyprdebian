/**
 * @file menu.hpp
 * @brief The menu: what hd-menu reads from stdin, as JSON.
 *
 * @code{.json}
 * {
 *     "banner": "Power off system?",
 *     "width": 400,
 *     "height": "30%",
 *     "options": [
 *         {"label": "No", "default": true},
 *         {"label": "Yes", "value": "yes", "icon": "/usr/share/hd-menu/poweroff.png"}
 *     ]
 * }
 * @endcode
 *
 * - banner: optional, a line of text at the top.
 * - width: optional, the width of the menu, in logical pixels (a number) or
 *   as a percentage of the output (a string such as "50%"); just enough for
 *   the banner and the options when there is none.
 * - height: optional, the height of the menu, likewise.
 * - options: the options, in this order, at least one; each has:
 *   - label: the text shown.
 *   - value: optional, what hd-menu prints when the option is chosen; the
 *     label when there is none.
 *   - icon: optional, the path of a PNG or SVG image (.svg or .svgz), shown
 *     left of the label at the icon_size of the theme.
 *   - default: optional, true for the option selected at the start; the
 *     first option when none has it.
 *
 * Other members are ignored.
 */
#pragma once

#include "icon.hpp"
#include "size.hpp"

#include <istream>
#include <string>
#include <string_view>
#include <vector>

/** @brief One option of the menu. */
struct Option {
    /** @brief The text shown. */
    std::string label;
    /** @brief What hd-menu prints when the option is chosen; the label when the JSON has none. */
    std::string value;
    /** @brief The icon left of the label, if any. */
    Icon icon;
};

/** @brief The menu, and which option is selected and shown. */
struct Menu {
    /** @brief The line of text at the top; none when empty. */
    std::string banner;
    /** @brief The width of the menu. */
    Size width;
    /** @brief The height of the menu. */
    Size height;
    /** @brief The options, at least one. */
    std::vector<Option> options;
    /** @brief The index in options of the option that is selected. */
    int selected = 0;
    /** @brief The index in options of the first option shown; the renderer keeps the selected one in view. */
    int scroll = 0;
};

/**
 * @brief Reads the menu, as JSON.
 *
 * Loads the icons. An icon that cannot be loaded is left out, with a message
 * on stderr.
 *
 * @param in The JSON of the menu, read to the end.
 * @return The menu, with the option that has default selected.
 * @throws std::runtime_error When the JSON is not valid, a size is not a
 *         number or a percentage, an option has no label, or there are no
 *         options.
 */
Menu read_menu(std::istream& in);

/**
 * @brief Lower cases a character, as xkbcommon does for keysyms.
 * @param c The character, as a Unicode code point.
 * @return The lower case character; c itself when it has none.
 */
char32_t lower_char(char32_t c);

/**
 * @brief Finds the option that typing selects.
 * @param menu  The menu.
 * @param typed What was typed, in lower case (see lower_char()).
 * @return The index in menu.options of the first option whose label, in
 *         lower case, starts with typed; -1 when there is none.
 */
int find_option(const Menu& menu, std::u32string_view typed);
