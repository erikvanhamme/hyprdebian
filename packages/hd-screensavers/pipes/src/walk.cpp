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

// The grid walk, after pinit, FindNeighbors, SelectNeighbor and
// generate_system of pipes.c; see walk.hpp.

#include "walk.hpp"

#include <algorithm>

Cell Cell::next(Direction d) const
{
    switch (d) {
    case Direction::up:
        return {x, y + 1, z};
    case Direction::down:
        return {x, y - 1, z};
    case Direction::left:
        return {x - 1, y, z};
    case Direction::right:
        return {x + 1, y, z};
    case Direction::near:
        return {x, y, z + 1};
    case Direction::far:
        return {x, y, z - 1};
    }
    return *this;
}

Walk::Walk(uint32_t seed, const WalkOptions& options)
    : options_(options)
    , random_(seed)
    , cells_(GRID_WIDTH * GRID_HEIGHT * GRID_DEPTH)
{
}

bool Walk::taken(Cell c) const
{
    return cells_[(c.x * GRID_HEIGHT + c.y) * GRID_DEPTH + c.z];
}

void Walk::take(Cell c)
{
    cells_[(c.x * GRID_HEIGHT + c.y) * GRID_DEPTH + c.z] = true;
}

int Walk::random(int n)
{
    return static_cast<int>(random_() % static_cast<uint32_t>(n));
}

std::vector<Direction> Walk::free_neighbours(Cell c) const
{
    std::vector<Direction> free;
    for (auto d : {Direction::up, Direction::down, Direction::left, Direction::right, Direction::near, Direction::far}) {
        if (!taken(c.next(d))) {
            free.push_back(d);
        }
    }
    return free;
}

Direction Walk::pick(const std::vector<Direction>& free)
{
    return free[random(static_cast<int>(free.size()))];
}

int Walk::pick_colour(std::array<int, COLOURS>& used)
{
    int lowest = *std::min_element(used.begin(), used.end());
    std::vector<int> least;
    for (int i = 0; i < COLOURS; i++) {
        if (used[i] == lowest) {
            least.push_back(i);
        }
    }
    int colour = least[random(static_cast<int>(least.size()))];
    used[colour]++;
    return colour;
}

Screenful Walk::next()
{
    // The wall: the outer layer of the grid is taken, so that no pipe leaves it.
    std::fill(cells_.begin(), cells_.end(), false);
    for (int x = 0; x < GRID_WIDTH; x++) {
        for (int y = 0; y < GRID_HEIGHT; y++) {
            for (int z = 0; z < GRID_DEPTH; z++) {
                if (x == 0 || x == GRID_WIDTH - 1 || y == 0 || y == GRID_HEIGHT - 1 || z == 0 || z == GRID_DEPTH - 1) {
                    take({x, y, z});
                }
            }
        }
    }

    Screenful screenful;
    std::array<int, COLOURS> used{};
    for (int number = 1; number <= options_.cycles; number++) {
        Piece::Kind joint = Piece::Kind::elbow;
        if (options_.joints == Joints::ball) {
            joint = Piece::Kind::ball;
        } else if (options_.joints == Joints::cycle) {
            // pipes.c takes turns among its three types: balls, bolted elbows and elbows. The bolts
            // are left out, so its second type has plain elbows.
            if ((number - 1) % 3 == 0) {
                joint = Piece::Kind::ball;
            }
        }
        int colour = pick_colour(used);
        screenful.systems.push_back(walk_system(colour, joint));
    }
    return screenful;
}

System Walk::walk_system(int colour, Piece::Kind joint)
{
    System system;
    system.colour = colour;
    system.joint = joint;

    // A free cell with a free neighbour. One always exists: a screenful takes at most 10 × 1002
    // cells of the 31 × 23 × 31 inside the wall, too few to leave every free cell boxed in.
    Cell cell;
    do {
        cell = {random(GRID_WIDTH - 1) + 1, random(GRID_HEIGHT - 1) + 1, random(GRID_DEPTH - 1) + 1};
    } while (taken(cell) || free_neighbours(cell).empty());
    take(cell);
    Direction now = pick(free_neighbours(cell));

    int counter = 0;
    int turncounter = 0;
    while (true) {
        Step step;
        auto free = free_neighbours(cell);
        if (system.steps.empty()) {
            step.pieces.push_back({.kind = Piece::Kind::start, .cell = cell});
        }
        if (free.empty() || counter > options_.size) {
            step.pieces.push_back({.kind = Piece::Kind::cap, .cell = cell});
            system.steps.push_back(std::move(step));
            return system;
        }
        counter++;
        turncounter++;

        Direction dir = now;
        if (std::find(free.begin(), free.end(), now) == free.end()) {
            dir = pick(free);
        } else if (options_.tightturns) {
            if (counter > 1 && random(100) < 20) {
                dir = pick(free);
            }
        } else if (counter > 1) {
            // The chance of a turn grows with each step since the last. pipes.c compares
            // NRAND(50) < NRAND(turncounter + 1) and leaves the order of the two to the compiler.
            int threshold = random(50);
            if (threshold < random(turncounter + 1)) {
                dir = pick(free);
                turncounter = 0;
            }
        }

        if (dir == now) {
            step.pieces.push_back({.kind = Piece::Kind::straight, .cell = cell, .out = dir});
        } else {
            step.pieces.push_back({.kind = joint, .cell = cell, .in = now, .out = dir});
        }
        step.pieces.push_back({.kind = Piece::Kind::link, .cell = cell, .out = dir});
        system.steps.push_back(std::move(step));

        now = dir;
        cell = cell.next(dir);
        take(cell);
    }
}
