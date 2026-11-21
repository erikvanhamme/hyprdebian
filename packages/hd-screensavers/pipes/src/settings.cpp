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

#include "settings.hpp"

void add_pipes_options(Options& options, PipesOptions& values)
{
    options.choice("joints", values.walk.joints,
                   {{"elbow", Joints::elbow}, {"ball", Joints::ball}, {"bolted", Joints::bolted},
                    {"cycle", Joints::cycle}},
                   "the joints where the pipes turn; cycle takes balls, bolted elbows and elbows by turns, one "
                   "kind per screenful");
    options.number("cycles", values.walk.cycles, 1, 10, "N", "pipes per screenful");
    options.number("size", values.walk.size, 10, 1000, "N", "the longest run of a pipe");
    options.number("delay", values.delay, 1, 1000000, "US", "the time of one step, in microseconds");
    options.flag("fisheye", values.fisheye, "the camera closer to the pipes");
    options.flag("tightturns", values.walk.tightturns, "the same chance of a turn at every step");
    options.number("factory", values.walk.factory, 0, 100, "N",
                   "the chance in percent of a gauge, a valve or the teapot on a straight piece; above 0 "
                   "the bolted elbows have their bolts");
    options.flag("rotatepipes", values.rotatepipes, "the pipes turn slowly");
    add_render_options(options, values.render);
    options.command_line_flag("window", values.window,
                              "one window instead of every monitor, which only Escape or closing it ends");
}
