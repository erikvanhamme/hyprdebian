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
//   hd-pipes [--window]
//
// It exits with 0 when it ends, with 1 when it cannot start (no Vulkan
// device, no layer shell, no outputs) or loses the compositor, and with 2 on
// a wrong argument.
//
// For now each monitor is black: the pipes come next. The rest is in
// ../../common/src, shared with the other screensavers: screen.cpp puts the
// surfaces on the outputs and handles the input, gpu.cpp picks the Vulkan
// device and swapchain.cpp gets the frames on a surface.

#include "gpu.hpp"
#include "program.hpp"
#include "screen.hpp"

#include <exception>
#include <getopt.h>
#include <iostream>
#include <memory>

const char* const PROGRAM = "hd-pipes";

namespace {

/** @brief A scene that draws nothing: the clear leaves the surface black. */
class Black : public Scene {
public:
    void draw(VkCommandBuffer, VkExtent2D) override {}
};

/** @brief Makes a black scene for every surface. */
class Pipes : public SceneMaker {
public:
    std::unique_ptr<Scene> make_scene(Gpu&, VkFormat) override { return std::make_unique<Black>(); }
};

void usage(std::ostream& out)
{
    out << "usage: hd-pipes [--window]\n"
           "  --window  one window instead of every monitor, which only Escape or closing it ends\n";
}

} // namespace

int main(int argc, char** argv)
{
    bool window = false;
    static const option options[] = {
        {"window", no_argument, nullptr, 'w'},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0},
    };
    int opt = 0;
    while ((opt = getopt_long(argc, argv, "", options, nullptr)) != -1) {
        switch (opt) {
        case 'w':
            window = true;
            break;
        case 'h':
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
        Pipes pipes;
        screen.run(gpu, pipes);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "hd-pipes: " << e.what() << "\n";
        return 1;
    }
}
