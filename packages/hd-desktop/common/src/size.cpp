#include "size.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

Size read_size(JsonReader& reader, const std::string& key)
{
    if (reader.peek() != '"') {
        const double value = reader.number();
        if (value < 0) {
            throw std::runtime_error(key + " is negative");
        }
        return {true, false, value};
    }
    const std::string text = reader.string();
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || std::string(end) != "%" || value < 0) {
        throw std::runtime_error(key + " is not a number of pixels or a percentage such as \"50%\"");
    }
    return {true, true, value};
}

double resolve(const Size& size, double output, double fit)
{
    double v = fit;
    if (size.set) {
        v = size.percent ? output * size.value / 100.0 : size.value;
    }
    return std::clamp(std::ceil(v), 0.0, output);
}
