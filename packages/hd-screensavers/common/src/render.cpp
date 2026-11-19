#include "render.hpp"

void add_render_options(Options& options, RenderOptions& values)
{
    options.number("framerate", values.framerate, 0, 1000, "N",
                   "the most frames per second; 0 for the refresh rate of the monitor");
    options.number("msaa", values.msaa, 1, 8, "N",
                   "the most samples per pixel against jagged edges: 1, 2, 4 or 8, the most the GPU has up to N");
}
