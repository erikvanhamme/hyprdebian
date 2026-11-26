/**
 * @file bounce.hpp
 * @brief The motion of the logo of hd-logo on one monitor, without Wayland
 *        or Vulkan: its size and step, its start, its ticks on the clock and
 *        its bounces.
 *
 * The logo moves at exactly 45°, on a grid: one step of s pixels on both
 * axes per tick. Rx and Ry are the distances it can travel, the width of the
 * monitor less that of the logo and its height less that of the logo; s
 * divides both, so the logo lands on the edges exactly. In steps the logo is
 * at x from 0 to Rx/s and y from 0 to Ry/s.
 *
 * At an edge it bounces and takes the next of the three colours. A corner
 * hit, the tick on which it reaches a vertical and a horizontal edge both,
 * is one bounce.
 */
#pragma once

#include <chrono>
#include <cstdint>
#include <random>

/** @brief The colours the logo goes through, one after the other: normal, highlight and light of the theme. */
constexpr int LOGO_COLOURS = 3;

/** @brief The size and step of the logo on a monitor, in pixels. */
struct Layout {
    /** @brief The size of the logo. */
    int width = 0;
    int height = 0;
    /** @brief How far it moves on both axes per tick. */
    int step = 1;
    /** @brief How far it can travel: the size of the monitor less that of the logo; step divides both. */
    int range_x = 0;
    int range_y = 0;
    /** @brief How often the ranges of the search were widened; 0 when a pair fits in the first ones. */
    int widened = 0;
};

/** @brief The share of the height of the monitor that the height of the logo aims at. */
constexpr double TARGET_HEIGHT = 0.20;
/** @brief How far the heights of the first search go from the target, as a share of the height of the monitor. */
constexpr double HEIGHT_SPREAD = 0.05;
/** @brief The speed the step aims at, in heights of the monitor per minute. */
constexpr double TARGET_SPEED = 10.0;
/** @brief How far the steps of the first search go from the target, as a share of it. */
constexpr double STEP_SPREAD = 0.25;

/**
 * @brief The step that the search aims at: TARGET_SPEED heights of the monitor per minute.
 * @param monitor_height The height of the monitor, in pixels.
 * @param rate           The ticks per second.
 * @return The step, in pixels per tick; not a whole number.
 */
double target_step(int monitor_height, double rate);

/**
 * @brief Finds the size and step of the logo on a monitor.
 *
 * Among the pairs of a height and a step where the step divides both
 * ranges, it takes the height nearest to TARGET_HEIGHT of the height of the
 * monitor, and of those the step nearest to target_step(); the width comes
 * from the aspect. The first search takes heights within HEIGHT_SPREAD of
 * the monitor from the target and steps within STEP_SPREAD of their target.
 * When no pair fits, each next search widens both by as much again, until
 * one does. A monitor too small for any logo that moves gets one that
 * stands still, with the ranges it has left.
 *
 * @param monitor_width  The width of the monitor, in pixels.
 * @param monitor_height The height of the monitor, in pixels.
 * @param aspect         The width of the logo divided by its height.
 * @param rate           The ticks per second.
 */
Layout find_layout(int monitor_width, int monitor_height, double aspect, double rate);

/** @brief What a tick did. */
enum class Event {
    none,   ///< The logo moved on.
    bounce, ///< It reached a vertical or a horizontal edge.
    corner, ///< It reached a vertical and a horizontal edge both.
};

/** @brief The logo on one monitor: where it is, where it goes and its colour. */
class Bounce {
public:
    /**
     * @brief A logo at a given start.
     * @param layout The size and step.
     * @param x      The column, in steps, from 0 to range_x / step.
     * @param y      The row, in steps, from 0 to range_y / step.
     * @param dx     The direction along x, 1 or -1; at an edge it is turned inwards.
     * @param dy     The direction along y, 1 or -1; at an edge it is turned inwards.
     * @param colour The colour, from 0 to LOGO_COLOURS - 1.
     */
    Bounce(const Layout& layout, int x, int y, int dx, int dy, int colour);

    /**
     * @brief A logo at a random start on a path that passes a corner.
     *
     * A random one of the four diagonal directions, a random position on the
     * grid from which that path reaches a corner, and a random colour. The
     * path reaches one when x0 - y0 is a multiple of the greatest common
     * divisor of the ranges in steps, on the diagonal from the top left to
     * the bottom right; on the other, when x0 + y0 is.
     */
    Bounce(const Layout& layout, std::mt19937& random);

    /** @brief Moves one step and bounces at the edges. */
    Event tick();

    /** @brief Where the top left corner of the logo is, in pixels. */
    int x() const { return x_ * layout_.step; }
    int y() const { return y_ * layout_.step; }

    /** @brief Where it is in steps. */
    int column() const { return x_; }
    int row() const { return y_; }

    /** @brief The directions, 1 or -1. */
    int dx() const { return dx_; }
    int dy() const { return dy_; }

    /** @brief The colour, from 0 to LOGO_COLOURS - 1. */
    int colour() const { return colour_; }

    /** @brief The size and step. */
    const Layout& layout() const { return layout_; }

private:
    /** @brief Turns the directions inwards at an edge. */
    void face_inwards();

    Layout layout_;
    /** @brief The last column and row, the ranges in steps. */
    int columns_ = 0;
    int rows_ = 0;
    int x_ = 0;
    int y_ = 0;
    int dx_ = 1;
    int dy_ = 1;
    int colour_ = 0;
};

/**
 * @brief The tick of a frame: the time since the first frame times the rate, rounded.
 *
 * The ticks follow the clock, not the frames, so a late frame skips to the
 * tick of its time. Rounded, a frame drawn a little before its turn, as the
 * frames of a frame rate are, still gets its own tick.
 *
 * @param elapsed The time since the first frame, which has tick 0.
 * @param rate    The ticks per second.
 */
int64_t tick_at(std::chrono::nanoseconds elapsed, double rate);
