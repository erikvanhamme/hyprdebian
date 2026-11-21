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

// The tests of the grid walk of hd-pipes, pipes/src/walk.cpp, after pipes.c.
// Each test walks screenfuls of many seeds and replays them on a grid of its
// own, to check where the pipes go: that they never overlap and stay in the
// grid, that they end after size + 1 steps or when boxed in, that their
// turns follow the rules with and without tightturns, that the equipment of
// the factory is only where pipes.c puts it, and that the same seed gives the
// same screenfuls.
//
// make test in the package directory compiles and runs it; it prints a line
// for every check that fails and exits with 1 when one did.

#include "walk.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <set>
#include <string>
#include <tuple>

namespace {

int failures = 0;

/** @brief Counts and prints a check that fails. */
void check(bool ok, const std::string& what)
{
    if (!ok) {
        failures++;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

/** @brief The seeds of each test. */
constexpr uint32_t SEEDS = 200;

bool opposite(Direction a, Direction b)
{
    // The directions come in pairs: up and down, left and right, near and far.
    return a != b && static_cast<int>(a) / 2 == static_cast<int>(b) / 2;
}

bool inside(Cell c)
{
    return c.x >= 1 && c.x <= GRID_WIDTH - 2 && c.y >= 1 && c.y <= GRID_HEIGHT - 2 && c.z >= 1 && c.z <= GRID_DEPTH - 2;
}

/** @brief The cells taken by the pipes of a screenful so far, as the test replays it. */
class Grid {
public:
    bool taken(Cell c) const { return !inside(c) || cells_.contains(key(c)); }
    void take(Cell c) { cells_.insert(key(c)); }

    bool boxed_in(Cell c) const
    {
        for (auto d : {Direction::up, Direction::down, Direction::left, Direction::right, Direction::near, Direction::far}) {
            if (!taken(c.next(d))) {
                return false;
            }
        }
        return true;
    }

private:
    static std::tuple<int, int, int> key(Cell c) { return {c.x, c.y, c.z}; }
    std::set<std::tuple<int, int, int>> cells_;
};

bool horizontal(Direction d)
{
    return d != Direction::up && d != Direction::down;
}

/** @brief Whether a piece is where the pipe goes straight on: a straight piece or equipment. */
bool straight_on(Piece::Kind k)
{
    return k == Piece::Kind::straight || k == Piece::Kind::gauge || k == Piece::Kind::valve || k == Piece::Kind::teapot;
}

/** @brief What the replay of a step sees: the cell, and whether straight on was free. */
struct Seen {
    const Piece& piece;
    Direction before;
    bool straight_free;
    /** @brief The steps of the pipe so far, this one included. */
    int counter;
    /** @brief Whether the cell above was free, before a gauge took it. */
    bool room;
    /** @brief The colour of the system. */
    int colour;
};

/**
 * @brief Replays a screenful on a grid of its own, checks its shape and
 *        calls @p each for every straight piece or joint.
 * @return The number of moves of each system.
 */
std::vector<int> replay(const Screenful& s, const WalkOptions& o, const std::string& name,
    const std::function<void(const Seen&)>& each = {})
{
    std::vector<int> moves;
    check(static_cast<int>(s.systems.size()) == o.cycles, name + ": a system per cycle");
    Grid grid;
    for (const auto& sys : s.systems) {
        if (sys.steps.empty() || sys.steps.front().pieces.empty()) {
            check(false, name + ": a system without steps");
            continue;
        }
        const Piece& start = sys.steps.front().pieces.front();
        check(start.kind == Piece::Kind::start, name + ": a system begins with the start");
        Cell cell = start.cell;
        check(inside(cell), name + ": the start is inside the wall");
        check(!grid.taken(cell), name + ": the start is in a free cell");
        grid.take(cell);

        Direction now = Direction::up;
        int counter = 0;
        for (size_t i = 0; i < sys.steps.size(); i++) {
            const auto& pieces = sys.steps[i].pieces;
            size_t first = i == 0 ? 1 : 0;
            if (i + 1 == sys.steps.size()) {
                check(pieces.size() == first + 1 && pieces.back().kind == Piece::Kind::cap && pieces.back().cell == cell,
                    name + ": the last step is the cap, in the last cell");
                break;
            }
            if (pieces.size() != first + 2) {
                check(false, name + ": a step is a piece and a link");
                break;
            }
            const Piece& piece = pieces[first];
            const Piece& link = pieces[first + 1];
            counter++;
            check(piece.cell == cell && link.cell == cell, name + ": the pieces of a step are in the cell the pipe is in");
            check(link.kind == Piece::Kind::link && link.out == piece.out, name + ": the link goes on where the piece goes");
            if (i == 0) {
                check(piece.kind == Piece::Kind::straight, name + ": the first step goes straight");
            } else if (straight_on(piece.kind)) {
                check(piece.out == now, name + ": a straight piece goes on in the same direction");
            } else {
                check(piece.kind == sys.joint, name + ": a turn has the joint of the system");
                check(piece.in == now, name + ": a joint comes in along the direction of the pipe");
                check(piece.out != now && !opposite(piece.out, now), name + ": a turn is of 90 degrees");
            }
            const Cell above = cell.next(Direction::up);
            const bool room = !grid.taken(above);
            if (piece.kind != Piece::Kind::straight && straight_on(piece.kind)) {
                check(o.factory > 0, name + ": equipment only with factory");
                check(counter > 1, name + ": equipment only from the second step of a pipe on");
            }
            if (piece.kind == Piece::Kind::gauge) {
                check(horizontal(piece.out), name + ": a gauge only on a horizontal pipe");
                check(room, name + ": a gauge only with a free cell above");
                check(piece.angle >= 45 && piece.angle < 315, name + ": the needle of a gauge from 45 to 314 degrees");
                grid.take(above);
            }
            if (piece.kind == Piece::Kind::valve || piece.kind == Piece::Kind::teapot) {
                const int turns = piece.out == Direction::near || piece.out == Direction::far ? 4 : 3;
                check(piece.turn >= 0 && piece.turn < turns, name + ": the turn of a valve or teapot of MakeValve");
            }
            if (piece.kind == Piece::Kind::valve) {
                check(piece.angle >= 0 && piece.angle < 90, name + ": the wheel of a valve from 0 to 89 degrees");
                check(piece.colour == RED || piece.colour == BLUE || piece.colour == YELLOW,
                      name + ": the wheel of a valve red, blue or yellow");
                check(piece.colour != sys.colour, name + ": the wheel of a valve in another colour than the pipe");
            }
            if (each && i > 0) {
                each({piece, now, !grid.taken(cell.next(now)), counter, room, sys.colour});
            }
            now = piece.out;
            cell = cell.next(now);
            check(inside(cell), name + ": the pipe stays inside the wall");
            check(!grid.taken(cell), name + ": the pipe goes into a free cell");
            grid.take(cell);
        }

        int n = static_cast<int>(sys.steps.size()) - 1;
        moves.push_back(n);
        check(n <= o.size + 1, name + ": a pipe has at most size + 1 steps");
        if (n < o.size + 1) {
            check(grid.boxed_in(cell), name + ": a pipe that ends before size + 1 steps is boxed in");
        }
    }
    return moves;
}

void test_shape(const WalkOptions& o, const std::string& name)
{
    for (uint32_t seed = 0; seed < SEEDS; seed++) {
        Walk walk(seed, o);
        for (int i = 0; i < 3; i++) {
            replay(walk.next(), o, name + " seed " + std::to_string(seed));
        }
    }
}

void test_ends()
{
    // With size 1000 and 10 systems some pipes are boxed in and others reach the end; replay checks
    // that each of them is one of the two.
    WalkOptions o{.cycles = 10, .size = 1000};
    int boxed = 0;
    int full = 0;
    for (uint32_t seed = 0; seed < SEEDS; seed++) {
        Walk walk(seed, o);
        for (int n : replay(walk.next(), o, "ends")) {
            (n == o.size + 1 ? full : boxed)++;
        }
    }
    check(full > 0, "ends: some pipes reach size + 1 steps");
    check(boxed > 0, "ends: some pipes are boxed in");
}

/**
 * @brief The share of steps that turn while straight on is free, after a
 *        step that did, and after one that went straight.
 */
std::pair<double, double> turn_rates(bool tightturns)
{
    WalkOptions o{.tightturns = tightturns};
    int after_turn = 0, turned_after_turn = 0;
    int after_straight = 0, turned_after_straight = 0;
    for (uint32_t seed = 0; seed < SEEDS; seed++) {
        Walk walk(seed, o);
        bool turned = false;
        replay(walk.next(), o, "turns", [&](const Seen& s) {
            bool turns = !straight_on(s.piece.kind);
            if (s.straight_free && s.counter > 2) {
                (turned ? after_turn : after_straight)++;
                if (turns) {
                    (turned ? turned_after_turn : turned_after_straight)++;
                }
            }
            turned = turns && s.straight_free;
        });
    }
    return {double(turned_after_turn) / after_turn, double(turned_after_straight) / after_straight};
}

void test_turns()
{
    // Without tightturns the chance is NRAND(50) < NRAND(turncounter + 1), which is 1 % right after
    // a turn; with it 20 %, of which a pick of the same direction takes a part.
    auto [loose_after_turn, loose_after_straight] = turn_rates(false);
    auto [tight_after_turn, tight_after_straight] = turn_rates(true);
    std::printf("turn rates: without tightturns %.3f after a turn, %.3f after a straight piece; "
                "with tightturns %.3f, %.3f\n",
        loose_after_turn, loose_after_straight, tight_after_turn, tight_after_straight);
    check(loose_after_turn < 0.03, "without tightturns a pipe seldom turns right after a turn");
    check(loose_after_straight > loose_after_turn * 2, "without tightturns the chance of a turn grows");
    check(tight_after_turn > 0.10 && tight_after_turn < 0.20, "with tightturns a pipe turns right after a turn as often as after a straight piece");
    check(tight_after_straight > 0.10 && tight_after_straight < 0.20, "with tightturns a pipe turns at 20 % of the steps, less the picks of the same direction");
}

void test_joints()
{
    // The joints of four screenfuls in a row; every system of a screenful has the same joint. Without
    // factory there are no bolts, as in pipes.c.
    using K = Piece::Kind;
    struct Case {
        Joints joints;
        int factory;
        std::vector<K> expected;
    };
    for (const Case& c : std::vector<Case>{
             {Joints::elbow, 2, {K::elbow, K::elbow, K::elbow, K::elbow}},
             {Joints::ball, 2, {K::ball, K::ball, K::ball, K::ball}},
             {Joints::bolted, 2, {K::bolted, K::bolted, K::bolted, K::bolted}},
             {Joints::bolted, 0, {K::elbow, K::elbow, K::elbow, K::elbow}},
             {Joints::cycle, 2, {K::ball, K::bolted, K::elbow, K::ball}},
             {Joints::cycle, 100, {K::ball, K::bolted, K::elbow, K::ball}},
             {Joints::cycle, 0, {K::ball, K::elbow, K::ball, K::elbow}},
         }) {
        WalkOptions o{.joints = c.joints, .cycles = 4, .size = 10, .factory = c.factory};
        Walk walk(1, o);
        const std::string name = "joints " + std::to_string(static_cast<int>(c.joints)) + ", factory " +
                                 std::to_string(c.factory);
        for (K joint : c.expected) {
            auto s = walk.next();
            for (const auto& sys : s.systems) {
                check(sys.joint == joint, name + ": the joint of the screenful");
            }
            replay(s, o, name);
        }
    }
}

/** @brief How often each kind of piece where a pipe goes straight on comes, from the second step of a pipe on. */
struct Factory {
    int straight = 0;
    int gauges = 0;
    int valves = 0;
    int teapots = 0;
    /** @brief The straight pieces of factory 100: a gauge without room, on a vertical pipe or under a taken cell. */
    int straight_without_room = 0;
};

Factory count_factory(const WalkOptions& o, const std::string& name)
{
    Factory f;
    for (uint32_t seed = 0; seed < SEEDS; seed++) {
        Walk walk(seed, o);
        for (int i = 0; i < 2; i++) {
            replay(walk.next(), o, name, [&](const Seen& s) {
                if (s.counter <= 1) {
                    return;
                }
                switch (s.piece.kind) {
                case Piece::Kind::straight:
                    f.straight++;
                    if (!horizontal(s.piece.out) || !s.room) {
                        f.straight_without_room++;
                    }
                    break;
                case Piece::Kind::gauge:
                    f.gauges++;
                    break;
                case Piece::Kind::valve:
                    f.valves++;
                    break;
                case Piece::Kind::teapot:
                    f.teapots++;
                    break;
                default:
                    break;
                }
            });
        }
    }
    return f;
}

void test_factory()
{
    // Without factory every straight piece is plain; replay checks that equipment only comes with it.
    const Factory none = count_factory({.factory = 0}, "factory 0");
    check(none.straight > 0 && none.gauges + none.valves + none.teapots == 0, "factory 0: no equipment");

    // At 100 every straight piece from the second step on is equipment, but a gauge that has no
    // room, which stays a straight piece: half the shapes are gauges, 48 % valves, 2 % teapots.
    const Factory all = count_factory({.cycles = 10, .size = 1000, .factory = 100}, "factory 100");
    const int shapes = all.gauges + all.valves + all.teapots + all.straight;
    std::printf("factory 100: %d gauges, %d valves, %d teapots, %d straight pieces\n", all.gauges, all.valves,
                all.teapots, all.straight);
    check(all.straight == all.straight_without_room, "factory 100: a straight piece only where a gauge has no room");
    check(all.gauges > 0 && all.teapots > 0, "factory 100: gauges, valves and teapots");
    check(std::abs(double(all.valves) / shapes - 0.48) < 0.02, "factory 100: 48 % valves");
    check(std::abs(double(all.teapots) / shapes - 0.02) < 0.01, "factory 100: 2 % teapots");
    check(std::abs(double(all.gauges + all.straight) / shapes - 0.5) < 0.02, "factory 100: 50 % gauges or no room");

    // The default, 2: a little equipment.
    const Factory some = count_factory({}, "factory 2");
    const double share = double(some.gauges + some.valves + some.teapots) / (some.gauges + some.valves + some.teapots +
                                                                             some.straight);
    std::printf("factory 2: %.4f of the straight pieces are equipment\n", share);
    check(share > 0.01 && share < 0.025, "factory 2: about 2 % of the straight pieces are equipment");
}

void test_colours()
{
    // The colours do not repeat on a screenful until all seven are used.
    WalkOptions o{.cycles = 10, .size = 10};
    for (uint32_t seed = 0; seed < SEEDS; seed++) {
        Walk walk(seed, o);
        auto s = walk.next();
        std::array<int, COLOURS> used{};
        for (const auto& sys : s.systems) {
            check(sys.colour >= 0 && sys.colour < COLOURS, "colours: a colour of the list");
            used[sys.colour]++;
        }
        for (int n : used) {
            check(n == 1 || n == 2, "colours: every colour once before any twice");
        }
    }
}

void test_seed()
{
    // Also with every straight piece equipment: the same seed gives the same factory.
    for (const WalkOptions& o : {WalkOptions{}, WalkOptions{.factory = 100}}) {
        Walk a(42, o);
        Walk b(42, o);
        Walk c(43, o);
        for (int i = 0; i < 3; i++) {
            auto sa = a.next();
            check(sa == b.next(), "seed: the same seed gives the same screenfuls");
            check(!(sa == c.next()), "seed: another seed gives other screenfuls");
        }
    }
}

} // namespace

int main()
{
    test_shape({}, "defaults");
    test_shape({.joints = Joints::ball, .cycles = 10, .size = 1000}, "10 systems of 1000");
    test_shape({.joints = Joints::elbow, .cycles = 1, .size = 10, .tightturns = true}, "1 system of 10, tightturns");
    test_shape({.joints = Joints::bolted, .cycles = 10, .size = 1000}, "10 bolted systems of 1000");
    test_shape({.cycles = 10, .size = 1000, .tightturns = true}, "10 systems of 1000, tightturns");
    test_shape({.cycles = 10, .size = 1000, .factory = 100}, "10 systems of 1000, factory 100");
    test_shape({.cycles = 10, .size = 1000, .tightturns = true, .factory = 100}, "10 systems of 1000, tightturns, factory 100");
    test_ends();
    test_turns();
    test_joints();
    test_factory();
    test_colours();
    test_seed();

    if (failures > 0) {
        std::printf("%d checks failed\n", failures);
        return 1;
    }
    std::printf("all checks passed\n");
    return 0;
}
