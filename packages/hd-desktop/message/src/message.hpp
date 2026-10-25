/**
 * @file message.hpp
 * @brief The message: what hd-message reads from stdin, as JSON.
 *
 * @code{.json}
 * {
 *     "title": "Reboot system?",
 *     "text": "The new kernel is used after a reboot.\nUnsaved work is lost.",
 *     "width": 600,
 *     "buttons": [
 *         {"label": "Later", "value": "later"},
 *         {"label": "Reboot", "value": "reboot", "icon": "/usr/share/icons/reboot.svg"}
 *     ],
 *     "default": "later"
 * }
 * @endcode
 *
 * - title: optional, a line of text at the top, in the big font.
 * - text: optional, the message, in the normal font. It is wrapped at the
 *   spaces to the width of the box; a newline starts a new line.
 * - width: optional, the width of the box, in logical pixels (a number) or
 *   as a percentage of the output (a string such as "50%"); without it, the
 *   box is as wide as the title, the text and the buttons need, but the text
 *   wraps at half the width of the output.
 * - buttons: the buttons, from left to right, at least one; each has:
 *   - label: the text shown.
 *   - value: optional, what hd-message prints when the button is chosen; the
 *     label when there is none.
 *   - icon: optional, the path of a PNG or SVG image (.svg or .svgz), shown
 *     left of the label at the icon_size of the theme.
 * - default: optional, the value of the button selected at the start; the
 *   first button when there is none.
 *
 * Other members are ignored.
 */
#pragma once

#include "icon.hpp"
#include "size.hpp"

#include <istream>
#include <string>
#include <vector>

/** @brief One button of the message. */
struct Button {
    /** @brief The text shown. */
    std::string label;
    /** @brief What hd-message prints when the button is chosen; the label when the JSON has none. */
    std::string value;
    /** @brief The icon left of the label, if any. */
    Icon icon;
};

/** @brief The message, and which button is selected. */
struct Message {
    /** @brief The line of text at the top; none when empty. */
    std::string title;
    /** @brief The text of the message; none when empty. */
    std::string text;
    /** @brief The width of the box. */
    Size width;
    /** @brief The buttons, at least one. */
    std::vector<Button> buttons;
    /** @brief The index in buttons of the button that is selected. */
    int selected = 0;
};

/**
 * @brief Reads the message, as JSON.
 *
 * Loads the icons. An icon that cannot be loaded is left out, with a message
 * on stderr.
 *
 * @param in The JSON of the message, read to the end.
 * @return The message, with the default button selected.
 * @throws std::runtime_error When the JSON is not valid, the width is not a
 *         number or a percentage, a button has no label, there are no
 *         buttons, or default is not the value of a button.
 */
Message read_message(std::istream& in);
