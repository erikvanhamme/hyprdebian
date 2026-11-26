/**
 * @file logo_options.hpp
 * @brief The options of hd-logo: the frame rate, and the window of the
 *        command line.
 *
 * Options of ../../common/src reads framerate from the key hd-logo of the
 * configuration and from the command line. The size, the speed and the
 * colours are not options: the size and the step come from the monitor,
 * see bounce.hpp, and the colours from the theme, see theme_colours.hpp.
 */
#pragma once

#include "options.hpp"

/** @brief The options of hd-logo, with their defaults. */
struct LogoOptions {
    /** @brief The ticks per second on each monitor, 1–1000; 0 for a tick at every refresh. */
    int framerate = 30;
    /** @brief Whether it is one window instead of every monitor; the command line only. */
    bool window = false;
};

/**
 * @brief Adds the options of hd-logo to options, each tied to its member of values.
 *
 * The defaults are the values of values when it is called.
 */
void add_logo_options(Options& options, LogoOptions& values);
