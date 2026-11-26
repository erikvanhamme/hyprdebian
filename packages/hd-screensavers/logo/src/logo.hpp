/**
 * @file logo.hpp
 * @brief The scenes of hd-logo: the logo bouncing on black, drawn with Vulkan.
 *
 * Every surface has a logo of its own, with its own size, step, start and
 * colour, from bounce.hpp. The scene finds the size and step at its first
 * frame, from the size of the surface and the ticks per second: framerate,
 * or with 0 the refresh rate of the output, 60 Hz while it is not known.
 * When either changes, as when the window is resized, it starts again.
 *
 * The logo is one quad, textured with the mask of logo_mask.h, which the
 * build renders from hd-logo.svg, filtered linearly between its mipmaps and
 * tinted with the colour of the logo; the attachments are cleared to black
 * before. The mask is one image on the device, shared by all scenes.
 */
#pragma once

#include "logo_options.hpp"
#include "screen.hpp"
#include "theme_colours.hpp"

#include <memory>

class LogoRenderer;

/** @brief Makes the scenes of hd-logo, and holds what they share on the device. */
class Logo : public SceneMaker {
public:
    /**
     * @param options The options, in their ranges.
     * @param palette The colours of the logo.
     */
    Logo(const LogoOptions& options, const LogoPalette& palette);

    /** @brief Destroys the mask and the pipelines; the scenes go first. */
    ~Logo() override;

    Logo(const Logo&) = delete;
    Logo& operator=(const Logo&) = delete;

    /**
     * @brief Makes the scene of a surface, with a random start.
     *
     * The first puts the mask on the device.
     *
     * @throws std::runtime_error When a Vulkan call fails.
     */
    std::unique_ptr<Scene> make_scene(Gpu& gpu, VkFormat format) override;

private:
    /** @brief The options. */
    LogoOptions options_;
    /** @brief The colours. */
    LogoPalette palette_;
    /** @brief The mask, its descriptor set and the pipelines; null until the first scene. */
    std::unique_ptr<LogoRenderer> renderer_;
};
