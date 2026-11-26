#include "logo_options.hpp"

void add_logo_options(Options& options, LogoOptions& values)
{
    options.number("framerate", values.framerate, 0, 1000, "N",
                   "the ticks per second, which set the step too; 0 for a tick at every refresh of the monitor");
    options.command_line_flag("window", values.window,
                              "one window instead of every monitor, which only Escape or closing it ends");
}
