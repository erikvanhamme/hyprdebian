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
// The options are those of pipes.c, with its names and defaults.
//
// It exits with 0 when it ends, with 1 when it cannot start (no Vulkan
// device, no layer shell, no outputs) or loses the compositor, and with 2 on
// a wrong argument.
//
// walk.cpp walks the grid, mesh.cpp makes the shapes of the pipes and
// pipes.cpp draws them, with the shaders pipe.vert and pipe.frag. The rest
// is in ../../common/src, shared with the other screensavers: screen.cpp
// puts the surfaces on the outputs and handles the input, gpu.cpp picks the
// Vulkan device and swapchain.cpp gets the frames on a surface.

#include "gpu.hpp"
#include "pipes.hpp"
#include "program.hpp"
#include "screen.hpp"

#include <charconv>
#include <exception>
#include <getopt.h>
#include <iostream>
#include <optional>
#include <string>

const char* const PROGRAM = "hd-pipes";

namespace {

void usage(std::ostream& out)
{
    out << "usage: hd-pipes [options]\n"
           "  --joints elbow|ball|cycle  the joints where the pipes turn; cycle takes turns per screenful (cycle)\n"
           "  --cycles N                 pipes per screenful, 1-10 (5)\n"
           "  --size N                   the longest run of a pipe, 10-1000 (500)\n"
           "  --delay US                 the time of one step, in microseconds, 1-1000000 (10000)\n"
           "  --fisheye, --no-fisheye    the camera closer to the pipes (on)\n"
           "  --tightturns, --no-tightturns\n"
           "                             the same chance of a turn at every step (off)\n"
           "  --rotatepipes, --no-rotatepipes\n"
           "                             the pipes turn slowly (on)\n"
           "  --window                   one window instead of every monitor, which only Escape or closing it ends\n";
}

/**
 * @brief Reads a whole number in a range.
 * @return The number; none when the text is not a number in the range.
 */
std::optional<int> number(const char* text, int lowest, int highest)
{
    const std::string s = text;
    int value = 0;
    const auto [end, error] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (error != std::errc() || end != s.data() + s.size() || value < lowest || value > highest) {
        return std::nullopt;
    }
    return value;
}

} // namespace

int main(int argc, char** argv)
{
    bool window = false;
    PipesOptions pipes_options;
    enum : int {
        JOINTS = 256,
        CYCLES,
        SIZE,
        DELAY,
        FISHEYE,
        NO_FISHEYE,
        TIGHTTURNS,
        NO_TIGHTTURNS,
        ROTATEPIPES,
        NO_ROTATEPIPES,
        WINDOW,
        HELP,
    };
    static const option options[] = {
        {"joints", required_argument, nullptr, JOINTS},
        {"cycles", required_argument, nullptr, CYCLES},
        {"size", required_argument, nullptr, SIZE},
        {"delay", required_argument, nullptr, DELAY},
        {"fisheye", no_argument, nullptr, FISHEYE},
        {"no-fisheye", no_argument, nullptr, NO_FISHEYE},
        {"tightturns", no_argument, nullptr, TIGHTTURNS},
        {"no-tightturns", no_argument, nullptr, NO_TIGHTTURNS},
        {"rotatepipes", no_argument, nullptr, ROTATEPIPES},
        {"no-rotatepipes", no_argument, nullptr, NO_ROTATEPIPES},
        {"window", no_argument, nullptr, WINDOW},
        {"help", no_argument, nullptr, HELP},
        {nullptr, 0, nullptr, 0},
    };
    int opt = 0;
    while ((opt = getopt_long(argc, argv, "", options, nullptr)) != -1) {
        std::optional<int> value;
        switch (opt) {
        case JOINTS:
            if (std::string(optarg) == "elbow") {
                pipes_options.walk.joints = Joints::elbow;
            } else if (std::string(optarg) == "ball") {
                pipes_options.walk.joints = Joints::ball;
            } else if (std::string(optarg) == "cycle") {
                pipes_options.walk.joints = Joints::cycle;
            } else {
                std::cerr << "hd-pipes: --joints is elbow, ball or cycle\n";
                return 2;
            }
            break;
        case CYCLES:
            value = number(optarg, 1, 10);
            if (!value) {
                std::cerr << "hd-pipes: --cycles is a number from 1 to 10\n";
                return 2;
            }
            pipes_options.walk.cycles = *value;
            break;
        case SIZE:
            value = number(optarg, 10, 1000);
            if (!value) {
                std::cerr << "hd-pipes: --size is a number from 10 to 1000\n";
                return 2;
            }
            pipes_options.walk.size = *value;
            break;
        case DELAY:
            value = number(optarg, 1, 1000000);
            if (!value) {
                std::cerr << "hd-pipes: --delay is a number of microseconds from 1 to 1000000\n";
                return 2;
            }
            pipes_options.delay = *value;
            break;
        case FISHEYE:
        case NO_FISHEYE:
            pipes_options.fisheye = opt == FISHEYE;
            break;
        case TIGHTTURNS:
        case NO_TIGHTTURNS:
            pipes_options.walk.tightturns = opt == TIGHTTURNS;
            break;
        case ROTATEPIPES:
        case NO_ROTATEPIPES:
            pipes_options.rotatepipes = opt == ROTATEPIPES;
            break;
        case WINDOW:
            window = true;
            break;
        case HELP:
            usage(std::cout);
            return 0;
        default:
            usage(std::cerr);
            return 2;
        }
    }
    if (optind < argc) {
        usage(std::cerr);
        return 2;
    }

    try {
        Screen screen(window);
        Gpu gpu(screen.display());
        Pipes pipes(pipes_options);
        screen.run(gpu, pipes);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "hd-pipes: " << e.what() << "\n";
        return 1;
    }
}
