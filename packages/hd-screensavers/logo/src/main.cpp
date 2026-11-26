// hd-logo: the moving logo of the old DVD players, with the Debian swirl and
// "HyprDebian" over it, a screensaver of HyprDebian for Hyprland and other
// compositors with wlr-layer-shell, drawn with Vulkan. It bounces on black
// on every monitor and takes the next colour of the theme at each bounce,
// until a key, the mouse or a touch, or SIGTERM, ends it; with --window it
// is one window, which Escape or closing it ends.
//
//   hd-logo [--framerate N] [--window]
//
// framerate, in logo_options.cpp, is also read from the key hd-logo of the
// configuration of the system and then that of the user, as
// {"framerate": 60}; the command line wins over both. The colours are
// normal, highlight and light of the key hd-theme, in theme_colours.cpp.
//
// It exits with 0 when it ends, with 1 when it cannot start (no Vulkan
// device, no layer shell, no outputs) or loses the compositor, and with 2 on
// a wrong argument.
//
// bounce.cpp finds the size and step of the logo on a monitor and moves it,
// and logo.cpp draws it, with the shaders logo.vert and logo.frag and the
// mask that the build makes of ../hd-logo.svg with rsvg-convert and
// ../mask.py. The rest is in ../../common/src, shared with the other
// screensavers: screen.cpp puts the surfaces on the outputs, handles the
// input and asks for the GPU of the compositor, gpu.cpp makes the Vulkan
// device that gpu_pick.cpp picks, swapchain.cpp gets the frames on a surface
// and options.cpp reads the options.

#include "gpu.hpp"
#include "logo.hpp"
#include "logo_options.hpp"
#include "options.hpp"
#include "program.hpp"
#include "screen.hpp"
#include "theme_colours.hpp"

#include <exception>
#include <iostream>
#include <string>
#include <vector>

const char* const PROGRAM = "hd-logo";

int main(int argc, char** argv)
{
    LogoOptions logo_options;
    Options options(PROGRAM);
    add_logo_options(options, logo_options);
    options.read_config(Options::config_files(), std::cerr);
    switch (options.parse(std::vector<std::string>(argv + 1, argv + argc), std::cout, std::cerr)) {
    case Options::Parsed::run:
        break;
    case Options::Parsed::help:
        return 0;
    case Options::Parsed::error:
        return 2;
    }
    const LogoPalette palette = read_palette(theme_files(), std::cerr);

    try {
        // One sample per pixel: the logo is one quad on whole pixels.
        Screen screen(logo_options.window, logo_options.framerate);
        Gpu gpu(screen.display(), screen.main_device(), 1);
        Logo logo(logo_options, palette);
        screen.run(gpu, logo);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "hd-logo: " << e.what() << "\n";
        return 1;
    }
}
