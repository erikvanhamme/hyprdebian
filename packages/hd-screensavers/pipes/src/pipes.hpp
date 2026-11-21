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
 * @file pipes.hpp
 * @brief The scenes of hd-pipes: the pipes of the grid walk, drawn with Vulkan.
 *
 * Every surface has a scene of its own, with a walk of its own seed. Each
 * scene shows one step of the walk per delay, as draw_pipes of pipes.c
 * shows one display list per frame; when the last step of a screenful is
 * shown, the scene shrinks and tilts by 90° in 25 steps and starts again
 * with the next screenful. With rotatepipes the scene turns about the Y
 * axis by 0.02° per step, from a random angle.
 *
 * The steps follow the clock, not the frames: a frame shows the steps of
 * the time that went by since the one before, and the turn and the shrink
 * are interpolated between two steps, so the speed is the same at any
 * refresh rate.
 *
 * The meshes of mesh.hpp are made once and shared by all scenes. Each piece
 * of a pipe is an instance of one of them, or of a few, as the bolted
 * elbows and the equipment of the factory, placed and turned as pipes.c
 * does; a scene writes the instances of a screenful into a buffer when it
 * starts one, and draws each mesh once per frame, with the instances of the
 * steps shown so far.
 *
 * The lighting is that of pipes.c in OpenGL 1.x, per pixel in the shaders
 * pipe.vert and pipe.frag.
 */
#pragma once

#include "screen.hpp"
#include "settings.hpp"

#include <memory>

class Renderer;

/** @brief Makes the scenes of hd-pipes, and holds what they share on the device. */
class Pipes : public SceneMaker {
public:
    /** @param options The options, in their ranges. */
    explicit Pipes(const PipesOptions& options);

    /** @brief Destroys the meshes and pipelines; the scenes go first. */
    ~Pipes() override;

    Pipes(const Pipes&) = delete;
    Pipes& operator=(const Pipes&) = delete;

    /**
     * @brief Makes the scene of a surface, with a random seed.
     *
     * The first makes the meshes on the device.
     *
     * @throws std::runtime_error When a Vulkan call fails.
     */
    std::unique_ptr<Scene> make_scene(Gpu& gpu, VkFormat format) override;

private:
    /** @brief The options. */
    PipesOptions options_;
    /** @brief The meshes and pipelines; null until the first scene. */
    std::unique_ptr<Renderer> renderer_;
};
