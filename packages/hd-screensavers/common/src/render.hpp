/**
 * @file render.hpp
 * @brief The options of the rendering that every screensaver has: the frame
 *        rate and the samples per pixel.
 *
 * Each screensaver adds them to its Options with add_render_options(), so
 * they are read from its key in the configuration and from the command line
 * as its own options are.
 */
#pragma once

#include "options.hpp"

/** @brief The options of the rendering, with their defaults. */
struct RenderOptions {
    /** @brief The most frames per second, 1–1000; 0 for the refresh rate of each monitor. */
    int framerate = 0;
    /** @brief The most samples per pixel, 1–8; the device takes 1, 2, 4 or 8, the most it has up to this. */
    int msaa = 8;
};

/**
 * @brief Adds the options framerate and msaa to options, each tied to its member of values.
 *
 * The defaults are the values of values when it is called.
 */
void add_render_options(Options& options, RenderOptions& values);
