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

// hd-pipes: the 3D pipes of Windows NT, a screensaver of HyprDebian for
// Hyprland and other compositors with wlr-layer-shell, drawn with Vulkan. It
// covers every monitor until a key, the mouse or a touch, or SIGTERM, ends
// it; with --window it is one window, which Escape or closing it ends.
//
//   hd-pipes [--joints elbow|ball|cycle] [--cycles N] [--size N] [--delay µs]
//            [--[no-]fisheye] [--[no-]tightturns] [--[no-]rotatepipes] [--window]
//
// The options are those of pipes.c, with its names and defaults, in
// settings.cpp. The key hd-pipes of the configuration of the system and then
// that of the user set them first, as {"joints": "ball", "fisheye": false};
// the command line wins over both.
//
// It exits with 0 when it ends, with 1 when it cannot start (no Vulkan
// device, no layer shell, no outputs) or loses the compositor, and with 2 on
// a wrong argument.
//
// walk.cpp walks the grid, mesh.cpp makes the shapes of the pipes and
// pipes.cpp draws them, with the shaders pipe.vert and pipe.frag. The rest
// is in ../../common/src, shared with the other screensavers: screen.cpp
// puts the surfaces on the outputs and handles the input, gpu.cpp picks the
// Vulkan device, swapchain.cpp gets the frames on a surface and options.cpp
// reads the options.

#include "gpu.hpp"
#include "options.hpp"
#include "pipes.hpp"
#include "program.hpp"
#include "screen.hpp"
#include "settings.hpp"

#include <exception>
#include <iostream>
#include <string>
#include <vector>

const char* const PROGRAM = "hd-pipes";

int main(int argc, char** argv)
{
    PipesOptions pipes_options;
    Options options(PROGRAM);
    add_pipes_options(options, pipes_options);
    options.read_config(Options::config_files(), std::cerr);
    switch (options.parse(std::vector<std::string>(argv + 1, argv + argc), std::cout, std::cerr)) {
    case Options::Parsed::run:
        break;
    case Options::Parsed::help:
        return 0;
    case Options::Parsed::error:
        return 2;
    }

    try {
        Screen screen(pipes_options.window);
        Gpu gpu(screen.display());
        Pipes pipes(pipes_options);
        screen.run(gpu, pipes);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "hd-pipes: " << e.what() << "\n";
        return 1;
    }
}
