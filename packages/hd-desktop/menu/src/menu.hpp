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

#include <cairo.h>
#include <istream>
#include <memory>
#include <string>
#include <vector>

/** @brief The handle of an SVG document of librsvg, declared here so that only menu.cpp and renderer.cpp include librsvg. */
typedef struct _RsvgHandle RsvgHandle;

/**
 * @brief The icon of an option.
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

/** @brief One option of the menu. */
struct Option {
    /** @brief The text shown. */
    std::string label;
    /** @brief What hd-menu prints when the option is chosen; the label when the JSON has none. */
    std::string value;
    /** @brief The icon left of the label, if any. */
    Icon icon;
};

/** @brief A width or a height of the menu. */
struct Size {
    /** @brief Whether the menu sets it; when not, the menu is just large enough for its content. */
    bool set = false;
    /** @brief Whether value is a percentage of the output instead of logical pixels. */
    bool percent = false;
    /** @brief Logical pixels, or a percentage of the output. */
    double value = 0;
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
