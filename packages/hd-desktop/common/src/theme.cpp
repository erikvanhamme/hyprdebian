#include "theme.hpp"

#include "json.hpp"
#include "program.hpp"

#include <algorithm>
#include <cstdlib>
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

// Reads the key hd-theme of the configuration in path into theme. False when
// the file does not exist, has no such key, or cannot be read.
bool read_theme(const std::string& path, Theme& theme)
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

    // The members of hd-theme, by type.
    bool found = false;
    std::map<std::string, std::string> strings;
    std::map<std::string, double> numbers;
    try {
        JsonReader reader(text.str());
        reader.object([&](const std::string& key) {
            if (key != "hd-theme" || reader.peek() != '{') {
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
    if (!found) {
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

} // namespace

Theme load_theme()
{
    for (const std::string& path : config_files()) {
        Theme theme;
        if (read_theme(path, theme)) {
            return theme;
        }
    }
    std::cerr << PROGRAM << ": no hd-theme in the configuration, using the default theme\n";
    return Theme();
}
