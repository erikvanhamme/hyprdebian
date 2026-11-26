/**
 * @file theme_colours.hpp
 * @brief The colours of hd-logo: normal, highlight and light of the theme.
 *
 * They come from the key hd-theme of the configuration as hd-menu and
 * hd-message read it: that of the user first, then that of the system; the
 * first configuration that has the key is taken. A colour it lacks, or one
 * that is not "#rrggbb", has its value of the theme default. They are read
 * once, at the start. dark is left out: on black it hardly shows.
 */
#pragma once

#include "bounce.hpp"

#include <array>
#include <ostream>
#include <string>
#include <vector>

/** @brief A colour, each channel from 0 to 1, as it goes on the screen. */
struct Rgb {
    float r = 0;
    float g = 0;
    float b = 0;
    bool operator==(const Rgb&) const = default;
};

/** @brief The colours of the logo, in the order it takes them. */
using LogoPalette = std::array<Rgb, LOGO_COLOURS>;

/** @brief normal, highlight and light of the theme default: #0066cc, #ff6600 and #e6edf5. */
LogoPalette default_palette();

/**
 * @brief The configurations in which the theme is looked up, first to last:
 *        that of the user, then that of the system; root has only that of
 *        the system.
 */
std::vector<std::string> theme_files();

/**
 * @brief Reads the colours from the key hd-theme of the first configuration that has it.
 *
 * A file that does not exist, or has no key hd-theme, is skipped quietly;
 * one that cannot be read or is not valid JSON is reported and skipped. A
 * colour that is not "#rrggbb" is reported and has its default.
 *
 * @param files  The configurations, the first to look in first, as theme_files().
 * @param errors Where the messages go.
 */
LogoPalette read_palette(const std::vector<std::string>& files, std::ostream& errors);
