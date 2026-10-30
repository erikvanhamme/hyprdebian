#include "theme.hpp"

#include "json.hpp"
#include "program.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <pwd.h>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

// "#rrggbb" to a colour; false when it is something else.
bool parse_color(const std::string& text, Color& color)
{
    if (text.size() != 7 || text[0] != '#') {
        return false;
    }
    char* end = nullptr;
    const long v = std::strtol(text.c_str() + 1, &end, 16);
    if (*end != '\0') {
        return false;
    }
    color = {((v >> 16) & 0xff) / 255.0, ((v >> 8) & 0xff) / 255.0, (v & 0xff) / 255.0};
    return true;
}

const char* const SYSTEM_CONFIG = "/etc/hyprdebian/config.json";

// The configurations in which the theme is looked up, first to last: for root
// the one of the system, for other users their own and then the one of the
// system, as hd-theme does.
std::vector<std::string> config_files()
{
    if (geteuid() == 0) {
        return {SYSTEM_CONFIG};
    }
    const passwd* user = getpwuid(getuid());
    if (user == nullptr || user->pw_dir == nullptr) {
        return {SYSTEM_CONFIG};
    }
    return {std::string(user->pw_dir) + "/.config/hyprdebian/config.json", SYSTEM_CONFIG};
}

// Reads the members of the object in the key name of the configuration in
// path, the strings and the numbers, by type. False when the file does not
// exist, has no such key, or cannot be read.
bool read_key(const std::string& path, const std::string& name, std::map<std::string, std::string>& strings,
              std::map<std::string, double>& numbers)
{
    if (access(path.c_str(), F_OK) != 0) {
        return false;
    }
    std::ifstream file(path);
    if (!file) {
        std::cerr << PROGRAM << ": cannot read " << path << "\n";
        return false;
    }
    std::stringstream text;
    text << file.rdbuf();

    bool found = false;
    try {
        JsonReader reader(text.str());
        reader.object([&](const std::string& key) {
            if (key != name || reader.peek() != '{') {
                reader.skip();
                return;
            }
            found = true;
            reader.object([&](const std::string& field) {
                const char c = reader.peek();
                if (c == '"') {
                    strings[field] = reader.string();
                } else if (c == '-' || (c >= '0' && c <= '9')) {
                    numbers[field] = reader.number();
                } else {
                    reader.skip();
                }
            });
        });
        reader.end();
    } catch (const std::exception& e) {
        std::cerr << PROGRAM << ": " << path << ": " << e.what() << "\n";
        return false;
    }
    return found;
}

// Reads the key hd-theme of the configuration in path into theme. False when
// the file does not exist, has no such key, or cannot be read.
bool read_theme(const std::string& path, Theme& theme)
{
    std::map<std::string, std::string> strings;
    std::map<std::string, double> numbers;
    if (!read_key(path, "hd-theme", strings, numbers)) {
        return false;
    }

    if (strings.count("font") != 0 && !strings["font"].empty()) {
        theme.font = strings["font"];
    }
    for (auto [name, color] : {std::pair{"highlight", &theme.highlight}, std::pair{"normal", &theme.normal},
                               std::pair{"dark", &theme.dark}, std::pair{"light", &theme.light}}) {
        if (strings.count(name) != 0 && !parse_color(strings[name], *color)) {
            std::cerr << PROGRAM << ": hd-theme: " << name << " is not a #rrggbb colour\n";
        }
    }
    if (numbers.count("font-size-big") != 0 && numbers["font-size-big"] > 0) {
        // Points to logical pixels: a point is 1/72 inch, a logical pixel 1/96.
        theme.font_size_big = numbers["font-size-big"] * 96.0 / 72.0;
    }
    if (numbers.count("font-size-normal") != 0 && numbers["font-size-normal"] > 0) {
        theme.font_size_normal = numbers["font-size-normal"] * 96.0 / 72.0;
    }
    if (numbers.count("icon_size") != 0 && numbers["icon_size"] > 0) {
        theme.icon_size = numbers["icon_size"];
    }
    if (numbers.count("border") != 0 && numbers["border"] >= 0) {
        theme.border = numbers["border"];
    }
    if (numbers.count("radius") != 0 && numbers["radius"] >= 0) {
        theme.radius = numbers["radius"];
    }
    if (numbers.count("opacity") != 0) {
        theme.opacity = std::clamp(numbers["opacity"], 0.0, 1.0);
    }
    return true;
}

// The sizes of a theme are made for a screen of this many pixels per inch,
// seen from this many centimetres, as in hd_display.py of hd-theme.
const double REFERENCE_DPI = 96;
const double REFERENCE_DISTANCE = 70;

// value rounded to digits decimals, halves up, as hd_display.py does.
double round_to(double value, int digits)
{
    const double step = std::pow(10.0, digits);
    return std::floor(value * step + 0.5) / step;
}

// The width and height in pixels of the preferred mode, which comes first, of
// the connected monitor with the most pixels; false when there is none.
bool monitor_size(double& width, double& height)
{
    bool found = false;
    std::error_code error;
    for (const auto& connector : std::filesystem::directory_iterator("/sys/class/drm", error)) {
        std::ifstream status(connector.path() / "status");
        std::string state;
        if (!std::getline(status, state) || state != "connected") {
            continue;
        }
        std::ifstream modes(connector.path() / "modes");
        std::string mode;
        int w = 0;
        int h = 0;
        if (!std::getline(modes, mode) || std::sscanf(mode.c_str(), "%dx%d", &w, &h) != 2) {
            continue;
        }
        if (!found || static_cast<double>(w) * h > width * height) {
            width = w;
            height = h;
            found = true;
        }
    }
    return found;
}

// The factor by which the sizes of the theme are multiplied, from the key
// hd-display of the configuration of the system, as hd_display.py works it
// out; 1 without the key, or with a message on stderr when it is not valid.
double scale_factor()
{
    std::map<std::string, std::string> strings;
    std::map<std::string, double> numbers;
    if (!read_key(SYSTEM_CONFIG, "hd-display", strings, numbers)) {
        return 1;
    }
    if (numbers.count("factor") != 0) {
        if (numbers["factor"] <= 0) {
            std::cerr << PROGRAM << ": hd-display: factor is not above 0, the theme is not scaled\n";
            return 1;
        }
        return round_to(numbers["factor"], 2);
    }
    if (numbers.count("diagonal") == 0 || numbers["diagonal"] <= 0 || numbers.count("distance") == 0
        || numbers["distance"] <= 0) {
        std::cerr << PROGRAM << ": hd-display: no factor, nor a diagonal and a distance above 0, the theme is not "
                  << "scaled\n";
        return 1;
    }
    double width = 0;
    double height = 0;
    if (!monitor_size(width, height)) {
        std::cerr << PROGRAM << ": no connected monitor in /sys/class/drm, the theme is not scaled\n";
        return 1;
    }
    const double dpi = std::hypot(width, height) / numbers["diagonal"];
    return round_to(dpi / REFERENCE_DPI * numbers["distance"] / REFERENCE_DISTANCE, 2);
}

// Multiplies the sizes of theme by factor, as hd-theme --apply does: the font
// sizes to a tenth of a point, the others to whole pixels, at least 1 when
// they are not 0.
void scale(Theme& theme, double factor)
{
    for (double* size : {&theme.font_size_big, &theme.font_size_normal}) {
        *size = round_to(*size * 72.0 / 96.0 * factor, 1) * 96.0 / 72.0;
    }
    for (double* size : {&theme.icon_size, &theme.border, &theme.radius}) {
        if (*size > 0) {
            *size = std::max(round_to(*size * factor, 0), 1.0);
        }
    }
}

} // namespace

Theme load_theme()
{
    Theme theme;
    bool found = false;
    for (const std::string& path : config_files()) {
        if (read_theme(path, theme)) {
            found = true;
            break;
        }
    }
    if (!found) {
        std::cerr << PROGRAM << ": no hd-theme in the configuration, using the default theme\n";
    }
    const double factor = scale_factor();
    if (factor != 1) {
        scale(theme, factor);
    }
    return theme;
}
