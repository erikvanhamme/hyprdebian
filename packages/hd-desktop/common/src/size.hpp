/**
 * @file size.hpp
 * @brief The width or the height of the box of hd-menu and hd-message.
 */
#pragma once

#include "json.hpp"

#include <string>

/** @brief A width or a height of the box. */
struct Size {
    /** @brief Whether the input sets it; when not, the box is just large enough for its content. */
    bool set = false;
    /** @brief Whether value is a percentage of the output instead of logical pixels. */
    bool percent = false;
    /** @brief Logical pixels, or a percentage of the output. */
    double value = 0;
};

/**
 * @brief Reads a size: a number of logical pixels, or a string with a percentage such as "50%".
 * @param reader The JSON, at the value of the size.
 * @param key    The name of the member, for the error.
 * @return The size, set.
 * @throws std::runtime_error When the value is neither, or is negative.
 */
Size read_size(JsonReader& reader, const std::string& key);

/**
 * @brief A size in whole logical pixels, no larger than the output.
 * @param size   The size the input sets, if any.
 * @param output The width or height of the output.
 * @param fit    What the content needs, for a size that is not set.
 * @return The size.
 */
double resolve(const Size& size, double output, double fit);
