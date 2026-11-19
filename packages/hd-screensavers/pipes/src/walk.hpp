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
 * @file walk.hpp
 * @brief The grid walk of the pipes, after pipes.c: where each pipe goes.
 *
 * The pipes run through a grid of 33×25×33 cells, whose outer layer is a
 * wall. A screenful is `cycles` systems on the same grid; each system is one
 * pipe of one colour. A pipe starts with a ball in a free cell that has a
 * free neighbour and goes on from cell to cell, a straight piece in every
 * cell it goes through and a link between each two. Where it turns the cell
 * gets a joint instead, an elbow or a ball. It ends, capped with a ball, when
 * it is boxed in or after `size` + 1 steps, as `counter > system_length` of
 * pipes.c has it.
 *
 * Without tightturns the chance of a turn grows with every step since the
 * last turn; with it, every step has a chance of 20 %. A pipe that cannot go
 * on straight always turns. A turn is always to a free neighbour, never back,
 * so every turn is of 90°.
 *
 * There is no Wayland or Vulkan in here: the renderer reveals the steps one
 * by one, one per `delay`, and starts the restart of the scene when the last
 * step of a screenful is shown.
 */
#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <vector>

/** @brief The cells of the grid along x, the outer two being the wall. */
constexpr int GRID_WIDTH = 33;
/** @brief The cells of the grid along y, the outer two being the wall. */
constexpr int GRID_HEIGHT = 25;
/** @brief The cells of the grid along z, the outer two being the wall. */
constexpr int GRID_DEPTH = 33;
/** @brief The colours a system can have; Colour in the renderer. */
constexpr int COLOURS = 7;

/**
 * @brief A direction of the grid, in the order of pipes.c, which the choice
 *        of a neighbour follows.
 */
enum class Direction : uint8_t {
    up,    ///< +y
    down,  ///< -y
    left,  ///< -x
    right, ///< +x
    near,  ///< +z, towards the camera
    far,   ///< -z
};

/** @brief A cell of the grid. */
struct Cell {
    int x = 0;
    int y = 0;
    int z = 0;

    bool operator==(const Cell&) const = default;

    /** @brief The neighbour in direction @p d. */
    Cell next(Direction d) const;
};

/** @brief The joints of the systems, the option joints. */
enum class Joints : uint8_t {
    elbow, ///< Every system has elbows, the type 3 of pipes.c.
    ball,  ///< Every system has balls, the type 1 of pipes.c.
    cycle, ///< The systems of a screenful take turns.
};

/** @brief The options of the walk, in the ranges of the options of hd-pipes. */
struct WalkOptions {
    Joints joints = Joints::cycle;
    /** @brief Systems per screenful, 1–10. */
    int cycles = 5;
    /** @brief The longest run of a pipe, 10–1000. */
    int size = 500;
    /** @brief Whether every step has the same chance of a turn. */
    bool tightturns = false;
};

/** @brief A piece of a pipe. */
struct Piece {
    enum class Kind : uint8_t {
        start,    ///< The ball at the start, in cell.
        straight, ///< A straight piece through the middle of cell, along out.
        link,     ///< A straight piece from cell to its neighbour in out, over the face between them.
        elbow,    ///< An elbow in cell, coming in along in and going on along out.
        ball,     ///< A ball in cell where the pipe turns, from in to out.
        cap,      ///< The ball at the end, in cell.
    };

    Kind kind;
    Cell cell;
    /** @brief The direction the pipe goes in when it comes into the cell; for elbow and ball. */
    Direction in = Direction::up;
    /** @brief The direction the pipe goes in when it leaves the cell; for straight, link, elbow and ball. */
    Direction out = Direction::up;

    bool operator==(const Piece&) const = default;
};

/**
 * @brief The pieces of one step of a pipe, shown at once, a display list of
 *        pipes.c.
 *
 * A step is a straight piece or a joint, and the link to the next cell; the
 * first also has the start, the last only the cap.
 */
struct Step {
    std::vector<Piece> pieces;

    bool operator==(const Step&) const = default;
};

/** @brief One pipe. */
struct System {
    /** @brief The colour, 0 to COLOURS - 1: red, green, blue, cyan, yellow, magenta, white. */
    int colour = 0;
    /** @brief The joint of its turns, elbow or ball. */
    Piece::Kind joint = Piece::Kind::elbow;
    std::vector<Step> steps;

    bool operator==(const System&) const = default;
};

/** @brief The systems of a screenful, in the order they are drawn. */
struct Screenful {
    std::vector<System> systems;

    bool operator==(const Screenful&) const = default;
};

/** @brief Walks the grid, screenful after screenful. */
class Walk {
public:
    /**
     * @brief Starts the walk.
     * @param seed    The seed of the random numbers; the same seed and options give the same screenfuls.
     * @param options The options, in their ranges.
     */
    Walk(uint32_t seed, const WalkOptions& options);

    /** @brief Walks the next screenful, on a cleared grid. */
    Screenful next();

private:
    /** @brief Whether a cell is taken, by the wall or a pipe. */
    bool taken(Cell c) const;

    /** @brief Marks a cell as taken. */
    void take(Cell c);

    /** @brief A random number from 0 to @p n - 1, NRAND of pipes.c. */
    int random(int n);

    /** @brief The free neighbours of a cell, in the order of Direction. */
    std::vector<Direction> free_neighbours(Cell c) const;

    /** @brief Picks one of @p free at random, SelectNeighbor of pipes.c. */
    Direction pick(const std::vector<Direction>& free);

    /**
     * @brief Picks the colour of a new system among the least used of the screenful.
     * @param used How often each colour is used in the screenful; the pick is counted.
     */
    int pick_colour(std::array<int, COLOURS>& used);

    /** @brief Walks one pipe from a random start, pinit and generate_system of pipes.c. */
    System walk_system(int colour, Piece::Kind joint);

    WalkOptions options_;
    std::mt19937 random_;
    /** @brief The cells, x first, then y, then z; true when taken. */
    std::vector<bool> cells_;
};
