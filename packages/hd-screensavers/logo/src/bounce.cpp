#include "bounce.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <tuple>

namespace {

/** @brief Room for the rounding of the bounds of the ranges, which are shares of whole numbers. */
constexpr double EPSILON = 1e-9;

} // namespace

double target_step(int monitor_height, double rate)
{
    return TARGET_SPEED * monitor_height / 60.0 / rate;
}

Layout find_layout(int monitor_width, int monitor_height, double aspect, double rate)
{
    const double target_height = TARGET_HEIGHT * monitor_height;
    const double target = target_step(monitor_height, rate);
    const int longest = std::max(monitor_width, monitor_height);

    for (int widened = 0;; widened++) {
        const double height_spread = HEIGHT_SPREAD * (widened + 1);
        const double step_spread = STEP_SPREAD * (widened + 1);
        const int lowest_height =
            std::max(1, static_cast<int>(std::ceil(monitor_height * (TARGET_HEIGHT - height_spread) - EPSILON)));
        const int highest_height = std::min(
            monitor_height - 1, static_cast<int>(std::floor(monitor_height * (TARGET_HEIGHT + height_spread) + EPSILON)));
        const int lowest_step = std::max(1, static_cast<int>(std::ceil(target * (1.0 - step_spread) - EPSILON)));
        const double highest_step = target * (1.0 + step_spread);
        const int highest_whole_step =
            static_cast<int>(std::min(std::floor(highest_step + EPSILON), static_cast<double>(longest)));

        // The nearest height first, then the nearest step; of two as near, the smaller.
        bool found = false;
        std::tuple<double, double, int, int> best;
        Layout layout;
        for (int height = lowest_height; height <= highest_height; height++) {
            const int width = std::max(1, static_cast<int>(std::lround(height * aspect)));
            const int range_x = monitor_width - width;
            const int range_y = monitor_height - height;
            if (range_x < 1 || range_y < 1) {
                continue;
            }
            const int divisor = std::gcd(range_x, range_y);
            for (int step = lowest_step; step <= std::min(highest_whole_step, divisor); step++) {
                if (divisor % step != 0) {
                    continue;
                }
                const auto key = std::make_tuple(std::abs(height - target_height), std::abs(step - target), height, step);
                if (!found || key < best) {
                    found = true;
                    best = key;
                    layout = {width, height, step, range_x, range_y, widened};
                }
            }
        }
        if (found) {
            return layout;
        }

        // Once the ranges hold every height and every step, widening finds nothing more: the
        // monitor is too small for a logo that moves.
        const bool every_height = lowest_height == 1 && highest_height == monitor_height - 1;
        const bool every_step = lowest_step == 1 && highest_step >= longest;
        if (every_height && every_step) {
            const int height = std::clamp(static_cast<int>(std::lround(target_height)), 1, std::max(1, monitor_height));
            const int width =
                std::clamp(static_cast<int>(std::lround(height * aspect)), 1, std::max(1, monitor_width));
            return {width, height, 1, std::max(0, monitor_width - width), std::max(0, monitor_height - height),
                    widened + 1};
        }
    }
}

Bounce::Bounce(const Layout& layout, int x, int y, int dx, int dy, int colour)
    : layout_(layout)
    , columns_(layout.range_x / layout.step)
    , rows_(layout.range_y / layout.step)
    , x_(std::clamp(x, 0, columns_))
    , y_(std::clamp(y, 0, rows_))
    , dx_(dx < 0 ? -1 : 1)
    , dy_(dy < 0 ? -1 : 1)
    , colour_(((colour % LOGO_COLOURS) + LOGO_COLOURS) % LOGO_COLOURS)
{
    face_inwards();
}

Bounce::Bounce(const Layout& layout, std::mt19937& random)
    : layout_(layout), columns_(layout.range_x / layout.step), rows_(layout.range_y / layout.step)
{
    std::uniform_int_distribution<int> sign(0, 1);
    dx_ = sign(random) == 0 ? -1 : 1;
    dy_ = sign(random) == 0 ? -1 : 1;
    colour_ = std::uniform_int_distribution<int>(0, LOGO_COLOURS - 1)(random);
    y_ = std::uniform_int_distribution<int>(0, rows_)(random);

    // A logo that cannot move along x is at an edge of x all the time, and reaches a corner at
    // each edge of y; one that cannot move along y the same.
    if (columns_ == 0 || rows_ == 0) {
        x_ = std::uniform_int_distribution<int>(0, columns_)(random);
    } else {
        // Unfolded, the logo moves along x' = dx x0 + t and y' = dy y0 + t, and is at an edge of x
        // when x' is a multiple of columns_ and of y when y' is one of rows_. Both happen at once
        // when dx x0 - dy y0 is a multiple of their greatest common divisor, so x0 is dx dy y0 in
        // the residues modulo it.
        const int divisor = std::gcd(columns_, rows_);
        const int residue = (((dx_ * dy_ * y_) % divisor) + divisor) % divisor;
        const int choices = (columns_ - residue) / divisor + 1;
        x_ = residue + divisor * std::uniform_int_distribution<int>(0, choices - 1)(random);
    }
    face_inwards();
}

void Bounce::face_inwards()
{
    if (x_ == 0) {
        dx_ = 1;
    } else if (x_ == columns_) {
        dx_ = -1;
    }
    if (y_ == 0) {
        dy_ = 1;
    } else if (y_ == rows_) {
        dy_ = -1;
    }
}

Event Bounce::tick()
{
    bool edge_x = false;
    bool edge_y = false;
    if (columns_ > 0) {
        x_ += dx_;
        if (x_ == 0 || x_ == columns_) {
            edge_x = true;
            dx_ = -dx_;
        }
    }
    if (rows_ > 0) {
        y_ += dy_;
        if (y_ == 0 || y_ == rows_) {
            edge_y = true;
            dy_ = -dy_;
        }
    }
    if (!edge_x && !edge_y) {
        return Event::none;
    }
    colour_ = (colour_ + 1) % LOGO_COLOURS;
    return edge_x && edge_y ? Event::corner : Event::bounce;
}

int64_t tick_at(std::chrono::nanoseconds elapsed, double rate)
{
    return std::llround(std::chrono::duration<double>(elapsed).count() * rate);
}
