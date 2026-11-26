#include "theme_colours.hpp"

#include "json.hpp"
#include "options.hpp"

#include <cctype>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <map>
#include <sstream>
#include <unistd.h>
#include <utility>

namespace {

/** @brief The start of the messages; the code is only in hd-logo, and the tests have no PROGRAM. */
constexpr const char* NAME = "hd-logo";

/** @brief "#rrggbb" to a colour; false when it is something else. */
bool parse_colour(const std::string& text, Rgb& colour)
{
    if (text.size() != 7 || text[0] != '#') {
        return false;
    }
    for (std::size_t i = 1; i < text.size(); i++) {
        if (std::isxdigit(static_cast<unsigned char>(text[i])) == 0) {
            return false;
        }
    }
    const long v = std::strtol(text.c_str() + 1, nullptr, 16);
    colour = {((v >> 16) & 0xff) / 255.0f, ((v >> 8) & 0xff) / 255.0f, (v & 0xff) / 255.0f};
    return true;
}

/**
 * @brief Reads the strings of the key hd-theme of the configuration in path.
 * @return False when the file does not exist, has no such key, or cannot be read.
 */
bool read_theme(const std::string& path, std::map<std::string, std::string>& strings, std::ostream& errors)
{
    if (access(path.c_str(), F_OK) != 0) {
        return false;
    }
    std::ifstream file(path);
    if (!file) {
        errors << NAME << ": cannot read " << path << "\n";
        return false;
    }
    std::stringstream text;
    text << file.rdbuf();

    bool found = false;
    try {
        JsonReader reader(text.str());
        reader.object([&](const std::string& key) {
            if (key != "hd-theme" || reader.peek() != '{') {
                reader.skip();
                return;
            }
            found = true;
            reader.object([&](const std::string& member) {
                if (reader.peek() == '"') {
                    strings[member] = reader.string();
                } else {
                    reader.skip();
                }
            });
        });
        reader.end();
    } catch (const std::exception& e) {
        errors << NAME << ": " << path << ": " << e.what() << "\n";
        strings.clear();
        return false;
    }
    return found;
}

} // namespace

LogoPalette default_palette()
{
    return {Rgb{0x00 / 255.0f, 0x66 / 255.0f, 0xcc / 255.0f}, Rgb{0xff / 255.0f, 0x66 / 255.0f, 0x00 / 255.0f},
            Rgb{0xe6 / 255.0f, 0xed / 255.0f, 0xf5 / 255.0f}};
}

std::vector<std::string> theme_files()
{
    // Options has them the other way round, the one below first.
    std::vector<std::string> files = Options::config_files();
    return {files.rbegin(), files.rend()};
}

LogoPalette read_palette(const std::vector<std::string>& files, std::ostream& errors)
{
    LogoPalette palette = default_palette();
    for (const std::string& path : files) {
        std::map<std::string, std::string> strings;
        if (!read_theme(path, strings, errors)) {
            continue;
        }
        const std::pair<const char*, std::size_t> names[] = {{"normal", 0}, {"highlight", 1}, {"light", 2}};
        for (const auto& [name, index] : names) {
            const auto found = strings.find(name);
            if (found != strings.end() && !parse_colour(found->second, palette[index])) {
                errors << NAME << ": " << path << ": hd-theme: " << name << " is not a #rrggbb colour\n";
            }
        }
        break;
    }
    return palette;
}
