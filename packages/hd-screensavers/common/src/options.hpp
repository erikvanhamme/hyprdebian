/**
 * @file options.hpp
 * @brief The options of a screensaver, from the configuration and the command line.
 *
 * Each screensaver lists its options in an Options, each tied to the
 * variable it sets, which holds the default when it is added. The values
 * then come in layers, each over the one before:
 *
 *   1. the defaults;
 *   2. the configuration of the system, /etc/hyprdebian/config.json;
 *   3. the configuration of the user, ~/.config/hyprdebian/config.json;
 *   4. the command line.
 *
 * In a configuration the options are the members of the key with the name
 * of the screensaver, an object with the long names of the options:
 * {"hd-pipes": {"joints": "ball", "fisheye": false}}. An option the user's
 * configuration leaves out keeps the value of the system's. A value out of
 * its range or of the wrong type is reported and skipped, so the layer
 * below stays; on the command line it is an error.
 *
 * On the command line each option is --name VALUE or --name=VALUE, and a
 * flag --name or --no-name. Options of the command line only, as --window,
 * are not read from a configuration.
 */
#pragma once

#include <cstddef>
#include <functional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

/** @brief The options of a screensaver, each tied to the variable it sets. */
class Options {
public:
    /**
     * @param name The name of the screensaver, as "hd-pipes": the key of
     *             its options in the configuration, and the start of its
     *             messages.
     */
    explicit Options(std::string name) : name_(std::move(name)) {}

    /**
     * @brief Adds a flag, --name and --no-name, true or false in a configuration.
     * @param value The variable it sets, with the default.
     * @param help What it does, for --help.
     */
    void flag(const std::string& name, bool& value, const std::string& help);

    /**
     * @brief Adds a flag of the command line only, --name, which sets value to true.
     * @param value The variable it sets.
     * @param help What it does, for --help.
     */
    void command_line_flag(const std::string& name, bool& value, const std::string& help);

    /**
     * @brief Adds a whole number from lowest to highest, --name N.
     * @param value The variable it sets, with the default.
     * @param metavar The name of the value in --help, as "N".
     * @param help What it does, for --help.
     */
    void number(const std::string& name, int& value, int lowest, int highest, const std::string& metavar,
                const std::string& help);

    /**
     * @brief Adds a choice of words, --name WORD.
     * @param value The variable it sets, with the default, which must be one of words.
     * @param words Each word with the value it stands for.
     * @param help What it does, for --help.
     */
    template <class T>
    void choice(const std::string& name, T& value, std::vector<std::pair<std::string, T>> words,
                const std::string& help)
    {
        std::vector<std::string> names;
        std::size_t current = 0;
        for (std::size_t i = 0; i < words.size(); ++i) {
            names.push_back(words[i].first);
            if (words[i].second == value) {
                current = i;
            }
        }
        add_choice(name, std::move(names), current, [&value, words](std::size_t i) { value = words[i].second; },
                   help);
    }

    /**
     * @brief The configurations of the user who runs the screensaver, from the
     *        one below to the one on top: that of the system, then that of the
     *        user. Root has only that of the system.
     */
    static std::vector<std::string> config_files();

    /**
     * @brief Reads the options from the configurations, each over the one before.
     *
     * A file that does not exist, or has no key with the name of the
     * screensaver, is skipped quietly. One that cannot be read or is not
     * valid JSON, a key that is not an object, a member that is not an
     * option, and a value of the wrong type or out of its range are
     * reported on errors and skipped.
     *
     * @param files The configurations, the one below first, as config_files().
     * @param errors Where the messages go.
     */
    void read_config(const std::vector<std::string>& files, std::ostream& errors);

    /** @brief What parse() found. */
    enum class Parsed {
        run,   ///< The options are set; run the screensaver.
        help,  ///< --help: usage() is printed on out; exit with 0.
        error, ///< A wrong argument, reported on errors with the usage; exit with 2.
    };

    /**
     * @brief Reads the options of the command line, over those of the configuration.
     * @param args The arguments, without the name of the program.
     * @param out Where --help prints the usage.
     * @param errors Where a wrong argument is reported.
     */
    Parsed parse(const std::vector<std::string>& args, std::ostream& out, std::ostream& errors);

    /** @brief Prints the usage, with every option and its default. */
    void usage(std::ostream& out) const;

private:
    /** @brief An option. */
    struct Option {
        enum class Kind { flag, command_line_flag, number, choice } kind;
        std::string name;
        /** @brief The value in --help, as "N" or "elbow|ball|cycle"; empty for a flag. */
        std::string metavar;
        std::string help;
        /** @brief The default, as --help shows it. */
        std::string fallback;
        /** @brief The range of a number. */
        int lowest = 0;
        int highest = 0;
        /** @brief The words of a choice. */
        std::vector<std::string> words;
        /** @brief Sets a flag. */
        std::function<void(bool)> set_flag;
        /** @brief Sets a number, or a choice by the index of its word. */
        std::function<void(int)> set_number;
    };

    void add_choice(const std::string& name, std::vector<std::string> words, std::size_t current,
                    std::function<void(std::size_t)> set, const std::string& help);

    /** @brief The option with the name; null when there is none. */
    const Option* find(const std::string& name) const;

    /** @brief What a value of option is, as "a whole number from 1 to 10" or "elbow, ball or cycle". */
    static std::string expected(const Option& option);

    /**
     * @brief Sets option from text, the value on the command line.
     * @return What is wrong with text; empty when it is set.
     */
    std::string set_from_text(const Option& option, const std::string& text) const;

    /** @brief Reads the key of the screensaver in one configuration. */
    void read_file(const std::string& path, std::ostream& errors);

    std::string name_;
    std::vector<Option> options_;
};
