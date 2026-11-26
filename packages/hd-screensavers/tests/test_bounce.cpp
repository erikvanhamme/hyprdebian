// The tests of the motion of hd-logo, logo/src/bounce.cpp: the search of the
// size and step on the monitors of the spec, 1366x768 among them, which
// needs the ranges widened; the start on a path that passes a corner; the
// bounces at each edge with the colour that moves on, and a corner hit; and
// the ticks on the clock at any frame rate, also when frames are late.
//
// make test in the package directory compiles and runs it; it prints a line
// for every check that fails and exits with 1 when one did.

#include "bounce.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <random>
#include <string>
#include <vector>

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

/** @brief The aspect of a square logo, as in the table of the spec. */
constexpr double SQUARE = 1.0;
/** @brief The aspect of the logo of hd-logo.svg, 234.8 x 178.66. */
constexpr double LOGO = 234.80014 / 178.6588;

struct Monitor {
    int width;
    int height;
};

/** @brief The monitors of the spec. */
const std::vector<Monitor> MONITORS = {{1920, 1080}, {2560, 1440}, {3840, 2160}, {1920, 1200},
                                       {3440, 1440}, {2560, 1080}, {1366, 768}};

std::string name(const Monitor& m, double aspect, double rate)
{
    return std::to_string(m.width) + "x" + std::to_string(m.height) + (aspect == SQUARE ? " square" : " logo") +
           " at " + std::to_string(static_cast<int>(rate));
}

/** @brief Checks what every layout must be: in the monitor, with a step that divides both ranges. */
void check_layout(const Layout& l, const Monitor& m, double aspect, double rate)
{
    const std::string what = "layout " + name(m, aspect, rate);
    check(l.height >= 1 && l.height < m.height, what + ": the height fits");
    check(l.width >= 1 && l.width < m.width, what + ": the width fits");
    check(l.width == static_cast<int>(std::lround(l.height * aspect)), what + ": the width from the aspect");
    check(l.range_x == m.width - l.width && l.range_y == m.height - l.height, what + ": the ranges");
    check(l.step >= 1 && l.range_x % l.step == 0 && l.range_y % l.step == 0,
          what + ": the step " + std::to_string(l.step) + " divides the ranges");
    const double target = target_step(m.height, rate);
    if (l.widened == 0) {
        check(l.height >= 0.15 * m.height - 1e-9 && l.height <= 0.25 * m.height + 1e-9,
              what + ": the height within 15-25%");
        check(l.step >= target * 0.75 - 1e-9 && l.step <= target * 1.25 + 1e-9, what + ": the step within 25%");
    } else {
        const double height_spread = HEIGHT_SPREAD * (l.widened + 1);
        const double step_spread = STEP_SPREAD * (l.widened + 1);
        check(std::abs(l.height - 0.2 * m.height) <= height_spread * m.height + 1e-9,
              what + ": the height within the widened range");
        check(l.step <= target * (1 + step_spread) + 1e-9 && (l.step == 1 || l.step >= target * (1 - step_spread) - 1e-9),
              what + ": the step within the widened range");
    }
}

void test_table()
{
    // The table of the spec, with a square logo and 30 frames a second.
    struct Row {
        Monitor monitor;
        int height;
        int step;
    };
    for (const Row& row : std::vector<Row>{{{1920, 1080}, 216, 6},
                                           {{2560, 1440}, 288, 8},
                                           {{3840, 2160}, 432, 12},
                                           {{1920, 1200}, 240, 6},
                                           {{3440, 1440}, 288, 8},
                                           {{2560, 1080}, 215, 5}}) {
        const Layout l = find_layout(row.monitor.width, row.monitor.height, SQUARE, 30);
        const std::string what = "table " + name(row.monitor, SQUARE, 30);
        check(l.height == row.height, what + ": height " + std::to_string(row.height) + ", not " +
                                          std::to_string(l.height));
        check(l.step == row.step, what + ": step " + std::to_string(row.step) + ", not " + std::to_string(l.step));
        check(l.widened == 0, what + ": not widened");
    }

    // 1366x768: no step of 4 or 5 divides both ranges of a square logo of 116-192 px.
    const Layout small = find_layout(1366, 768, SQUARE, 30);
    check(small.widened > 0, "table 1366x768 square: the ranges are widened");
}

void test_layouts()
{
    for (const Monitor& m : MONITORS) {
        for (double aspect : {SQUARE, LOGO}) {
            for (double rate : {1.0, 24.0, 30.0, 59.951, 60.0, 144.0, 1000.0}) {
                check_layout(find_layout(m.width, m.height, aspect, rate), m, aspect, rate);
            }
        }
    }
    // The nearest height first: no height nearer to 20% has a step in the range that divides both ranges.
    for (const Monitor& m : MONITORS) {
        for (double aspect : {SQUARE, LOGO}) {
            const Layout l = find_layout(m.width, m.height, aspect, 30);
            if (l.widened != 0) {
                continue;
            }
            const double target = target_step(m.height, 30);
            bool nearest = true;
            for (int h = 1; h < m.height; h++) {
                if (std::abs(h - 0.2 * m.height) >= std::abs(l.height - 0.2 * m.height) || h < 0.15 * m.height ||
                    h > 0.25 * m.height) {
                    continue;
                }
                const int rx = m.width - static_cast<int>(std::lround(h * aspect));
                const int ry = m.height - h;
                for (int st = 1; st <= target * 1.25; st++) {
                    if (st >= target * 0.75 && rx % st == 0 && ry % st == 0) {
                        nearest = false;
                    }
                }
            }
            check(nearest, "layout " + name(m, aspect, 30) + ": the nearest height that has a step, " +
                               std::to_string(l.height));
        }
    }

    // A window too small for a logo that moves gets one that stands still.
    const Layout tiny = find_layout(1, 1, LOGO, 30);
    check(tiny.width == 1 && tiny.height == 1 && tiny.range_x == 0 && tiny.range_y == 0 && tiny.step == 1,
          "layout 1x1: a logo that stands still");
    Bounce still(tiny, 0, 0, 1, 1, 0);
    check(still.tick() == Event::none && still.x() == 0 && still.y() == 0, "layout 1x1: it does not move");
}

/** @brief The ticks until the path of a logo repeats: it is back where it started, going the same way. */
int64_t period(const Bounce& b)
{
    const int columns = b.layout().range_x / b.layout().step;
    const int rows = b.layout().range_y / b.layout().step;
    return std::lcm(2 * static_cast<int64_t>(columns), 2 * static_cast<int64_t>(rows));
}

void test_start()
{
    std::mt19937 random(12345);
    for (const Monitor& m : MONITORS) {
        for (double aspect : {SQUARE, LOGO}) {
            const Layout l = find_layout(m.width, m.height, aspect, 30);
            const int columns = l.range_x / l.step;
            const int rows = l.range_y / l.step;
            const int divisor = std::gcd(columns, rows);
            bool all_conditions = true;
            bool all_corners = true;
            bool all_on_grid = true;
            int directions = 0;
            int colours = 0;
            for (int i = 0; i < 100; i++) {
                Bounce b(l, random);
                all_on_grid = all_on_grid && b.column() >= 0 && b.column() <= columns && b.row() >= 0 &&
                              b.row() <= rows && b.x() == b.column() * l.step && b.y() == b.row() * l.step;
                // x0 - y0 on the diagonal from the top left to the bottom right, x0 + y0 on the other.
                const int sum = b.dx() == b.dy() ? b.column() - b.row() : b.column() + b.row();
                all_conditions = all_conditions && sum % divisor == 0;
                directions |= 1 << ((b.dx() > 0 ? 1 : 0) + (b.dy() > 0 ? 2 : 0));
                colours |= 1 << b.colour();
                bool corner = false;
                for (int64_t t = 0; t < period(b) && !corner; t++) {
                    corner = b.tick() == Event::corner;
                }
                all_corners = all_corners && corner;
            }
            const std::string what = "start " + name(m, aspect, 30);
            check(all_on_grid, what + ": on the grid");
            check(all_conditions, what + ": x0 -/+ y0 is a multiple of gcd(Rx, Ry) in steps");
            check(all_corners, what + ": the path passes a corner");
            check(directions == 0xf, what + ": all four directions");
            check(colours == 0x7, what + ": all three colours");
        }
    }

    // A start on an edge goes inwards.
    const Layout l = find_layout(1920, 1080, SQUARE, 30);
    Bounce left(l, 0, 10, -1, 1, 0);
    check(left.dx() == 1, "start: on the left edge it goes right");
    Bounce bottom(l, 10, l.range_y / l.step, 1, 1, 0);
    check(bottom.dy() == -1, "start: on the bottom edge it goes up");
}

void test_bounces()
{
    // 1920x1080 square: 216 px, step 6, 284 columns and 144 rows.
    const Layout l = find_layout(1920, 1080, SQUARE, 30);
    const int columns = l.range_x / l.step;
    const int rows = l.range_y / l.step;
    check(columns == 284 && rows == 144, "bounces: 284 columns and 144 rows");

    // From the middle down to the right: the bottom comes first, after 72 - 1 ticks of nothing.
    Bounce b(l, 100, 72, 1, 1, 0);
    for (int i = 0; i < 71; i++) {
        if (b.tick() != Event::none) {
            check(false, "bounces: nothing before the bottom");
            break;
        }
    }
    check(b.colour() == 0, "bounces: the colour stays without a bounce");
    check(b.tick() == Event::bounce && b.row() == rows && b.y() == l.range_y, "bounces: lands on the bottom edge");
    check(b.dy() == -1 && b.dx() == 1, "bounces: the bottom turns it up");
    check(b.colour() == 1, "bounces: the bottom moves on to the next colour");
    check(b.tick() == Event::none && b.row() == rows - 1, "bounces: moves up from the bottom");

    // On to the right edge, at column 284: 284 - 173 = 111 ticks more.
    const int to_right = columns - b.column();
    for (int i = 0; i < to_right - 1; i++) {
        b.tick();
    }
    check(b.tick() == Event::bounce && b.column() == columns && b.x() == l.range_x, "bounces: lands on the right edge");
    check(b.dx() == -1, "bounces: the right turns it left");
    check(b.colour() == 2, "bounces: the right moves on to the next colour");

    // The top and the left, with the colour coming round to the first again.
    Bounce top(l, 50, 1, 1, -1, 2);
    check(top.tick() == Event::bounce && top.row() == 0 && top.dy() == 1, "bounces: the top turns it down");
    check(top.colour() == 0, "bounces: after the last colour the first");
    Bounce left(l, 1, 50, -1, 1, 1);
    check(left.tick() == Event::bounce && left.column() == 0 && left.dx() == 1, "bounces: the left turns it right");
    check(left.colour() == 2, "bounces: the left moves on to the next colour");

    // A corner hit: both edges on the same tick, one colour on, both directions turned.
    Bounce corner(l, columns - 1, rows - 1, 1, 1, 0);
    check(corner.tick() == Event::corner && corner.column() == columns && corner.row() == rows,
          "bounces: a corner hit at the bottom right");
    check(corner.dx() == -1 && corner.dy() == -1, "bounces: a corner turns both directions");
    check(corner.colour() == 1, "bounces: a corner moves on one colour, as a bounce for now");
    Bounce top_left(l, 1, 1, -1, -1, 2);
    check(top_left.tick() == Event::corner && top_left.x() == 0 && top_left.y() == 0,
          "bounces: a corner hit at the top left");
}

void test_ticks()
{
    using std::chrono::duration_cast;
    using std::chrono::nanoseconds;
    using Seconds = std::chrono::duration<double>;

    for (double rate : {1.0, 24.0, 30.0, 59.951, 60.0, 144.0, 1000.0}) {
        const std::string what = "ticks at " + std::to_string(rate);
        check(tick_at(nanoseconds(0), rate) == 0, what + ": the first frame is tick 0");
        // Frames on their turns, and drawn up to 2 ms early, as a frame rate draws them, or a
        // little late, get their own tick.
        bool own = true;
        bool early = true;
        bool late = true;
        for (int64_t k = 0; k < 2000; k++) {
            const Seconds turn(k / rate);
            own = own && tick_at(duration_cast<nanoseconds>(turn), rate) == k;
            early = early && tick_at(duration_cast<nanoseconds>(turn - Seconds(std::min(0.002, 0.4 / rate))), rate) == k;
            late = late && tick_at(duration_cast<nanoseconds>(turn + Seconds(0.4 / rate)), rate) == k;
        }
        check(own, what + ": a frame on its turn gets its tick");
        check(early, what + ": a frame drawn early gets its tick");
        check(late, what + ": a frame a little late gets its tick");
        // A frame that is late by 10 ticks skips to the tick of its time.
        check(tick_at(duration_cast<nanoseconds>(Seconds(10.0 / rate)), rate) == 10,
              what + ": a late frame skips to the tick of its time");
    }
    // An hour at 30 ticks a second.
    check(tick_at(std::chrono::hours(1), 30) == 108000, "ticks: an hour at 30 is 108000 ticks");
}

} // namespace

int main()
{
    test_table();
    test_layouts();
    test_start();
    test_bounces();
    test_ticks();

    if (failures > 0) {
        std::printf("%d checks failed\n", failures);
        return 1;
    }
    std::printf("all checks passed\n");
    return 0;
}
