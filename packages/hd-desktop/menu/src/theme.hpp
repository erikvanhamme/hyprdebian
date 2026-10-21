/**
 * @file theme.hpp
 * @brief The look of the menu.
 *
 * The theme is the "hd-theme" key of the configuration of the user,
 * ~/.config/hyprdebian/config.json, or, when that has none, of the
 * configuration of the system, /etc/hyprdebian/config.json; hd-theme --select
 * sets them. Root only reads the one of the system. Every field that is
 * missing keeps its default.
 */
#pragma once

#include <string>

/** @brief A colour, with each channel from 0 to 1. */
struct Color {
    /** @brief Red. */
    double r;
    /** @brief Green. */
    double g;
    /** @brief Blue. */
    double b;
};

/**
 * @brief The settings of the hd-theme that hd-menu uses, with their defaults.
 *
 * Sizes are in logical pixels.
 */
struct Theme {
    /** @brief The font of all text, a family name for fontconfig. */
    std::string font = "JetBrainsMono NFM Medium";
    /**
     * @brief The size of all text: font-size-big.
     *
     * The theme has it in points; load_theme() converts it to logical pixels
     * at 96 dpi, the resolution of a logical pixel. The theme also has
     * font-size-normal and font-size-small; hd-menu uses the big one.
     */
    double font_size_big = 32;  // 24 pt
    /** @brief The width and height of the icons of the options. */
    double icon_size = 64;
    /** @brief Focus: the outline of the focused window. Never a fill behind text. */
    Color highlight = {1.0, 0.4, 0.0};  // #ff6600
    /**
     * @brief The background of the selected option, the borders of what has
     *        no focus and thin divider lines.
     */
    Color normal = {0.0, 0.4, 0.8};     // #0066cc
    /** @brief The background of the window, drawn with opacity as alpha. */
    Color dark = {0x07 / 255.0, 0x14 / 255.0, 0x22 / 255.0};  // #071422
    /** @brief All text. */
    Color light = {0xe6 / 255.0, 0xed / 255.0, 0xf5 / 255.0}; // #e6edf5
    /** @brief The width of the outline of the window. */
    double border = 4;
    /** @brief The corner radius of the outline of the window. */
    double radius = 10;
    /** @brief The opacity of the background of the window only, from 0 to 1. */
    double opacity = 0.8;
};

/**
 * @brief Reads the theme of the user who runs hd-menu.
 *
 * The first configuration that has the key "hd-theme" gives the theme. A
 * configuration that does not exist, or has no such key, is skipped
 * quietly; one that cannot be read or is not valid JSON is skipped with a
 * message on stderr. Fields that are missing or invalid keep their default;
 * an invalid colour is reported on stderr.
 *
 * @return The theme, or the defaults, with a message on stderr, when no
 *         configuration has one.
 */
Theme load_theme();
