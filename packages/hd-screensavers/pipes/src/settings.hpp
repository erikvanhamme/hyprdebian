/*
 * Based on hacks/glx/pipes.c of xscreensaver, from xlockmore, which carries
 * this notice:
 *
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 *
 *   This file is provided AS IS with no warranties of any kind.  The author
 *   shall have no liability with respect to the infringement of copyrights,
 *   trade secrets or any patents by this file or any part thereof.  In no
 *   event will the author be liable for any lost revenue or profits or
 *   other special, indirect and consequential damages.
 *
 *   This program was inspired on a WindowsNT(R)'s screen saver. It was written
 *   from scratch and it was not based on any other source code.
 *
 *   ==========================================================================
 *   The routine myElbow is derivated from the doughnut routine from the MesaGL
 *   library (more especifically the Mesaaux library) written by Brian Paul.
 *   ==========================================================================
 *
 *   Thanks goes to Brian Paul for making it possible and inexpensive to use
 *   OpenGL at home.
 *
 *   Since I'm not a native English speaker, my apologies for any grammatical
 *   mistake.
 *
 *   My e-mail address is
 *   m-vianna@usa.net
 *   Marcelo F. Vianna (Apr-09-1997)
 */

/**
 * @file settings.hpp
 * @brief The options of hd-pipes: their names, ranges and defaults, those of
 *        pipes.c, and the options of the rendering of ../../common/src/render.hpp.
 *
 * Options of ../../common/src reads them from the key hd-pipes of the
 * configuration and from the command line.
 */
#pragma once

#include "options.hpp"
#include "render.hpp"
#include "walk.hpp"

/** @brief The options of hd-pipes, with their defaults. */
struct PipesOptions {
    /** @brief The options of the walk. */
    WalkOptions walk;
    /** @brief The time of one step, in µs, 1–1000000. */
    int delay = 10000;
    /** @brief Whether the camera is closer, 3.8 away instead of 4.8. */
    bool fisheye = true;
    /** @brief Whether the scene turns about the Y axis. */
    bool rotatepipes = true;
    /** @brief The frame rate and the samples per pixel. */
    RenderOptions render;
    /** @brief Whether it is one window instead of every monitor; the command line only. */
    bool window = false;
};

/**
 * @brief Adds the options of hd-pipes to options, each tied to its member of values.
 *
 * The defaults are the values of values when it is called.
 */
void add_pipes_options(Options& options, PipesOptions& values);
