#include "options.hpp"

#include "json.hpp"

#include <charconv>
#include <cmath>
#include <fstream>
#include <pwd.h>
#include <sstream>
#include <unistd.h>

namespace {

const char* const SYSTEM_CONFIG = "/etc/hyprdebian/config.json";

// The width of the options in the usage, before their help.
const std::size_t USAGE_COLUMN = 29;

// "a, b or c".
std::string either(const std::vector<std::string>& words)
{
    std::string text;
    for (std::size_t i = 0; i < words.size(); ++i) {
        if (i > 0) {
            text += i + 1 == words.size() ? " or " : ", ";
        }
        text += words[i];
    }
    return text;
}

// The text with words joined by separator.
std::string joined(const std::vector<std::string>& words, const std::string& separator)
{
    std::string text;
    for (std::size_t i = 0; i < words.size(); ++i) {
        if (i > 0) {
            text += separator;
        }
        text += words[i];
    }
    return text;
}

} // namespace

void Options::flag(const std::string& name, bool& value, const std::string& help)
{
    options_.push_back({
        .kind = Option::Kind::flag,
        .name = name,
        .help = help,
        .fallback = value ? "on" : "off",
        .set_flag = [&value](bool v) { value = v; },
    });
}

void Options::command_line_flag(const std::string& name, bool& value, const std::string& help)
{
    options_.push_back({
        .kind = Option::Kind::command_line_flag,
        .name = name,
        .help = help,
        .set_flag = [&value](bool v) { value = v; },
    });
}

void Options::number(const std::string& name, int& value, int lowest, int highest, const std::string& metavar,
                     const std::string& help)
{
    options_.push_back({
        .kind = Option::Kind::number,
        .name = name,
        .metavar = metavar,
        .help = help,
        .fallback = std::to_string(value),
        .lowest = lowest,
        .highest = highest,
        .set_number = [&value](int v) { value = v; },
    });
}

void Options::add_choice(const std::string& name, std::vector<std::string> words, std::size_t current,
                         std::function<void(std::size_t)> set, const std::string& help)
{
    options_.push_back({
        .kind = Option::Kind::choice,
        .name = name,
        .metavar = joined(words, "|"),
        .help = help,
        .fallback = words[current],
        .words = std::move(words),
        .set_number = [set = std::move(set)](int i) { set(static_cast<std::size_t>(i)); },
    });
}

std::vector<std::string> Options::config_files()
{
    if (geteuid() == 0) {
        return {SYSTEM_CONFIG};
    }
    const passwd* user = getpwuid(getuid());
    if (user == nullptr || user->pw_dir == nullptr) {
        return {SYSTEM_CONFIG};
    }
    return {SYSTEM_CONFIG, std::string(user->pw_dir) + "/.config/hyprdebian/config.json"};
}

const Options::Option* Options::find(const std::string& name) const
{
    for (const auto& option : options_) {
        if (option.name == name) {
            return &option;
        }
    }
    return nullptr;
}

void Options::read_config(const std::vector<std::string>& files, std::ostream& errors)
{
    for (const auto& path : files) {
        read_file(path, errors);
    }
}

void Options::read_file(const std::string& path, std::ostream& errors)
{
    if (access(path.c_str(), F_OK) != 0) {
        return;
    }
    std::ifstream file(path);
    if (!file) {
        errors << name_ << ": cannot read " << path << "\n";
        return;
    }
    std::stringstream text;
    text << file.rdbuf();

    // The values are set as they are read, so a file that turns out not to be
    // valid JSON further on is not read at all: they go here first.
    std::vector<std::function<void()>> sets;
    std::vector<std::string> wrong;
    try {
        JsonReader reader(text.str());
        reader.object([&](const std::string& key) {
            if (key != name_) {
                reader.skip();
                return;
            }
            if (reader.peek() != '{') {
                wrong.push_back(name_ + " is not an object");
                reader.skip();
                return;
            }
            reader.object([&](const std::string& member) {
                const Option* option = find(member);
                if (option == nullptr || option->kind == Option::Kind::command_line_flag) {
                    wrong.push_back(member + " is not an option of " + name_);
                    reader.skip();
                    return;
                }
                const char c = reader.peek();
                if (option->kind == Option::Kind::flag && (c == 't' || c == 'f')) {
                    sets.push_back([option, v = reader.boolean()] { option->set_flag(v); });
                    return;
                }
                if (option->kind == Option::Kind::number && (c == '-' || (c >= '0' && c <= '9'))) {
                    const double v = reader.number();
                    if (v == std::floor(v) && v >= option->lowest && v <= option->highest) {
                        sets.push_back([option, v] { option->set_number(static_cast<int>(v)); });
                    } else {
                        wrong.push_back(member + " is " + expected(*option));
                    }
                    return;
                }
                if (option->kind == Option::Kind::choice && c == '"') {
                    const std::string v = reader.string();
                    for (std::size_t i = 0; i < option->words.size(); ++i) {
                        if (option->words[i] == v) {
                            sets.push_back([option, i] { option->set_number(static_cast<int>(i)); });
                            return;
                        }
                    }
                    wrong.push_back(member + " is " + expected(*option));
                    return;
                }
                wrong.push_back(member + " is " + expected(*option));
                reader.skip();
            });
        });
        reader.end();
    } catch (const std::exception& e) {
        errors << name_ << ": " << path << ": " << e.what() << "\n";
        return;
    }
    for (const auto& what : wrong) {
        errors << name_ << ": " << path << ": " << what << ", skipped\n";
    }
    for (const auto& set : sets) {
        set();
    }
}

std::string Options::expected(const Option& option)
{
    switch (option.kind) {
    case Option::Kind::number:
        return "a whole number from " + std::to_string(option.lowest) + " to " + std::to_string(option.highest);
    case Option::Kind::choice:
        return either(option.words);
    case Option::Kind::flag:
    case Option::Kind::command_line_flag:
        break;
    }
    return "true or false";
}

std::string Options::set_from_text(const Option& option, const std::string& text) const
{
    switch (option.kind) {
    case Option::Kind::number: {
        int value = 0;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error != std::errc() || end != text.data() + text.size() || value < option.lowest
            || value > option.highest) {
            return expected(option);
        }
        option.set_number(value);
        return "";
    }
    case Option::Kind::choice:
        for (std::size_t i = 0; i < option.words.size(); ++i) {
            if (option.words[i] == text) {
                option.set_number(static_cast<int>(i));
                return "";
            }
        }
        return expected(option);
    case Option::Kind::flag:
    case Option::Kind::command_line_flag:
        break;
    }
    return "";
}

Options::Parsed Options::parse(const std::vector<std::string>& args, std::ostream& out, std::ostream& errors)
{
    auto wrong = [&](const std::string& what) {
        errors << name_ << ": " << what << "\n";
        usage(errors);
        return Parsed::error;
    };
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (!arg.starts_with("--") || arg == "--") {
            return wrong("unexpected argument " + arg);
        }
        std::string name = arg.substr(2);
        std::string value;
        const auto equals = name.find('=');
        const bool has_value = equals != std::string::npos;
        if (has_value) {
            value = name.substr(equals + 1);
            name.resize(equals);
        }

        if (name == "help") {
            if (has_value) {
                return wrong("--help takes no value");
            }
            usage(out);
            return Parsed::help;
        }
        const Option* option = find(name);
        bool on = true;
        if (option == nullptr && name.starts_with("no-")) {
            option = find(name.substr(3));
            if (option != nullptr && option->kind != Option::Kind::flag) {
                option = nullptr;
            }
            on = false;
        }
        if (option == nullptr) {
            return wrong("unknown option --" + name);
        }

        if (option->kind == Option::Kind::flag || option->kind == Option::Kind::command_line_flag) {
            if (has_value) {
                return wrong("--" + name + " takes no value");
            }
            option->set_flag(on);
            continue;
        }
        if (!has_value) {
            if (i + 1 == args.size()) {
                return wrong("--" + name + " needs a value");
            }
            value = args[++i];
        }
        const std::string what = set_from_text(*option, value);
        if (!what.empty()) {
            return wrong("--" + name + " is " + what);
        }
    }
    return Parsed::run;
}

void Options::usage(std::ostream& out) const
{
    out << "usage: " << name_ << " [options]\n";
    auto line = [&](const std::string& left, const std::string& help) {
        std::string text = "  " + left;
        if (text.size() + 2 > USAGE_COLUMN) {
            text += "\n";
            text.append(USAGE_COLUMN, ' ');
        } else {
            text.append(USAGE_COLUMN - text.size(), ' ');
        }
        out << text << help << "\n";
    };
    for (const auto& option : options_) {
        switch (option.kind) {
        case Option::Kind::flag:
            line("--" + option.name + ", --no-" + option.name, option.help + " (" + option.fallback + ")");
            break;
        case Option::Kind::command_line_flag:
            line("--" + option.name, option.help);
            break;
        case Option::Kind::number:
            line("--" + option.name + " " + option.metavar, option.help + ", " + std::to_string(option.lowest)
                                                                + "-" + std::to_string(option.highest) + " ("
                                                                + option.fallback + ")");
            break;
        case Option::Kind::choice:
            line("--" + option.name + " " + option.metavar, option.help + " (" + option.fallback + ")");
            break;
        }
    }
    line("--help", "this text");
}
