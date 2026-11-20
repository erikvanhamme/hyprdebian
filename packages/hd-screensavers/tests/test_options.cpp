/*
 * Based on hacks/glx/pipes.c of xscreensaver, from xlockmore, which carries
 * this notice:
 *
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 *
 *   This file is provided AS IS with no warranties of any kind.  The author
 *   shall have no liability with respect to the infringement of copyrights,
 *   trade secrets or any patents by this file or any part thereof.  In no
 *   event will the author be liable for any lost revenue or profits or
 *   other special, indirect and consequential damages.
 *
 *   This program was inspired on a WindowsNT(R)'s screen saver. It was written
 *   from scratch and it was not based on any other source code.
 *
 *   ==========================================================================
 *   The routine myElbow is derivated from the doughnut routine from the MesaGL
 *   library (more especifically the Mesaaux library) written by Brian Paul.
 *   ==========================================================================
 *
 *   Thanks goes to Brian Paul for making it possible and inexpensive to use
 *   OpenGL at home.
 *
 *   Since I'm not a native English speaker, my apologies for any grammatical
 *   mistake.
 *
 *   My e-mail address is
 *   m-vianna@usa.net
 *   Marcelo F. Vianna (Apr-09-1997)
 */

// The tests of the options of hd-pipes: Options of common/src/options.cpp
// with the options of pipes/src/settings.cpp and common/src/render.cpp. They check the layers, the
// defaults, the configuration of the system, that of the user and the
// command line, each over the one before, and that values out of range or
// of the wrong type are skipped in a configuration and refused on the
// command line. The configurations are files in a temporary directory.
//
// make test in the package directory compiles and runs it; it prints a line
// for every check that fails and exits with 1 when one did.

#include "options.hpp"
#include "settings.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
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

/** @brief The temporary directory of the configurations. */
std::filesystem::path directory;

/** @brief Writes text to the file name in the directory, and gives its path. */
std::string config(const std::string& name, const std::string& text)
{
    const auto path = directory / name;
    std::ofstream(path) << text;
    return path.string();
}

/** @brief What a run of the options gives. */
struct Result {
    PipesOptions values;
    Options::Parsed parsed = Options::Parsed::run;
    /** @brief The messages of the configurations and the command line. */
    std::string errors;
    /** @brief What --help printed. */
    std::string out;
};

/** @brief Reads the configurations files, the one below first, and then the command line args. */
Result run(const std::vector<std::string>& files, const std::vector<std::string>& args = {})
{
    Result result;
    Options options("hd-pipes");
    add_pipes_options(options, result.values);
    std::ostringstream out;
    std::ostringstream errors;
    options.read_config(files, errors);
    result.parsed = options.parse(args, out, errors);
    result.errors = errors.str();
    result.out = out.str();
    return result;
}

bool contains(const std::string& text, const std::string& part)
{
    return text.find(part) != std::string::npos;
}

void test_defaults()
{
    const Result r = run({(directory / "missing.json").string()});
    check(r.parsed == Options::Parsed::run, "defaults: runs");
    check(r.errors.empty(), "defaults: a missing configuration is skipped quietly");
    check(r.values.walk.joints == Joints::cycle, "defaults: joints cycle");
    check(r.values.walk.cycles == 5, "defaults: cycles 5");
    check(r.values.walk.size == 500, "defaults: size 500");
    check(r.values.delay == 10000, "defaults: delay 10000");
    check(r.values.fisheye, "defaults: fisheye on");
    check(!r.values.walk.tightturns, "defaults: tightturns off");
    check(r.values.walk.factory == 2, "defaults: factory 2");
    check(r.values.rotatepipes, "defaults: rotatepipes on");
    check(r.values.render.framerate == 0, "defaults: framerate 0, the refresh rate");
    check(r.values.render.msaa == 8, "defaults: msaa 8");
    check(!r.values.window, "defaults: no window");
}

void test_system()
{
    const auto system = config("system.json", R"({"hd-theme": {"font": "x"},
        "hd-pipes": {"joints": "ball", "cycles": 3, "size": 20, "delay": 500,
                     "fisheye": false, "tightturns": true, "rotatepipes": false,
                     "framerate": 30, "msaa": 4, "factory": 0}})");
    const Result r = run({system});
    check(r.errors.empty(), "system: no messages");
    check(r.values.walk.joints == Joints::ball, "system: joints");
    check(r.values.walk.cycles == 3, "system: cycles");
    check(r.values.walk.size == 20, "system: size");
    check(r.values.delay == 500, "system: delay");
    check(!r.values.fisheye, "system: fisheye");
    check(r.values.walk.tightturns, "system: tightturns");
    check(!r.values.rotatepipes, "system: rotatepipes");
    check(r.values.render.framerate == 30, "system: framerate");
    check(r.values.render.msaa == 4, "system: msaa");
    check(r.values.walk.factory == 0, "system: factory");

    const Result other = run({config("other.json", R"({"hd-theme": {"font": "x"}})")});
    check(other.errors.empty() && other.values.walk.cycles == 5, "system: a configuration without the key is skipped");
}

void test_user_over_system()
{
    const auto system = config("system.json", R"({"hd-pipes": {"joints": "ball", "cycles": 3, "fisheye": false}})");
    const auto user = config("user.json", R"({"hd-pipes": {"cycles": 7, "fisheye": true}})");
    const Result r = run({system, user});
    check(r.errors.empty(), "user: no messages");
    check(r.values.walk.cycles == 7, "user: wins over the system");
    check(r.values.fisheye, "user: a flag wins over the system");
    check(r.values.walk.joints == Joints::ball, "user: what it leaves out keeps the value of the system");
    check(r.values.walk.size == 500, "user: what neither has keeps the default");
}

void test_command_line()
{
    const auto system = config("system.json", R"({"hd-pipes": {"joints": "ball", "cycles": 3, "size": 20}})");
    const auto user = config("user.json", R"({"hd-pipes": {"cycles": 7, "fisheye": false}})");
    const Result r = run({system, user}, {"--cycles", "9", "--size=40", "--fisheye", "--no-rotatepipes", "--window",
                                          "--framerate", "60", "--msaa=2", "--factory", "100"});
    check(r.parsed == Options::Parsed::run, "command line: runs");
    check(r.errors.empty(), "command line: no messages");
    check(r.values.walk.cycles == 9, "command line: wins over the user");
    check(r.values.walk.size == 40, "command line: wins over the system, with =");
    check(r.values.fisheye, "command line: a flag wins over the user");
    check(!r.values.rotatepipes, "command line: --no-");
    check(r.values.window, "command line: --window");
    check(r.values.render.framerate == 60, "command line: --framerate");
    check(r.values.render.msaa == 2, "command line: --msaa");
    check(r.values.walk.factory == 100, "command line: --factory");
    check(r.values.walk.joints == Joints::ball, "command line: what it leaves out keeps the configuration");

    const Result last = run({}, {"--joints", "elbow", "--joints", "ball", "--no-fisheye", "--fisheye"});
    check(last.values.walk.joints == Joints::ball && last.values.fisheye, "command line: the last one wins");

    const Result bolted = run({config("bolted.json", R"({"hd-pipes": {"joints": "elbow"}})")}, {"--joints", "bolted"});
    check(bolted.errors.empty() && bolted.values.walk.joints == Joints::bolted, "command line: --joints bolted");
    const Result config_bolted = run({config("bolted.json", R"({"hd-pipes": {"joints": "bolted"}})")});
    check(config_bolted.errors.empty() && config_bolted.values.walk.joints == Joints::bolted,
          "config: joints bolted");
}

void test_config_out_of_range()
{
    const auto system = config("system.json", R"({"hd-pipes": {"cycles": 3, "size": 20, "delay": 500,
        "joints": "ball", "fisheye": false}})");
    const auto user = config("user.json", R"({"hd-pipes": {"cycles": 11, "size": 9, "delay": 0.5,
        "joints": "teapot", "fisheye": "no", "window": true, "bolts": true, "tightturns": true,
        "framerate": 1001, "msaa": "4", "factory": 101}})");
    const Result r = run({system, user});
    check(r.parsed == Options::Parsed::run, "config range: runs");
    check(r.values.walk.cycles == 3, "config range: cycles 11 keeps the value of the system");
    check(r.values.walk.size == 20, "config range: size 9 keeps the value of the system");
    check(r.values.delay == 500, "config range: delay 0.5 keeps the value of the system");
    check(r.values.walk.joints == Joints::ball, "config range: joints teapot keeps the value of the system");
    check(!r.values.fisheye, "config range: a string for a flag keeps the value of the system");
    check(!r.values.window, "config range: window is of the command line only");
    check(r.values.walk.tightturns, "config range: the valid values of the same key are set");
    check(r.values.render.framerate == 0, "config range: framerate 1001 keeps the default");
    check(r.values.render.msaa == 8, "config range: msaa as a string keeps the default");
    check(r.values.walk.factory == 2, "config range: factory 101 keeps the default");
    for (const char* what : {"cycles is a whole number from 1 to 10", "size is a whole number from 10 to 1000",
                             "delay is a whole number from 1 to 1000000", "joints is elbow, ball, bolted or cycle",
                             "fisheye is true or false", "window is not an option of hd-pipes",
                             "bolts is not an option of hd-pipes", "framerate is a whole number from 0 to 1000",
                             "msaa is a whole number from 1 to 8", "factory is a whole number from 0 to 100"}) {
        check(contains(r.errors, user + ": " + what + ", skipped"), std::string("config range: reports ") + what);
    }

    const Result upper = run({config("upper.json",
                                     R"({"hd-pipes": {"cycles": 10, "size": 1000, "delay": 1000000, "factory": 100}})")});
    check(upper.errors.empty() && upper.values.walk.cycles == 10 && upper.values.walk.size == 1000
              && upper.values.delay == 1000000 && upper.values.walk.factory == 100,
          "config range: the highest values are in range");
    const Result lower = run({config("lower.json",
                                     R"({"hd-pipes": {"cycles": 1, "size": 10, "delay": 1, "factory": 0}})")});
    check(lower.errors.empty() && lower.values.walk.cycles == 1 && lower.values.walk.size == 10
              && lower.values.delay == 1 && lower.values.walk.factory == 0,
          "config range: the lowest values are in range");
}

void test_config_broken()
{
    const auto system = config("system.json", R"({"hd-pipes": {"cycles": 3}})");
    const auto user = config("user.json", R"({"hd-pipes": {"cycles": 7, "size": 20)");
    const Result r = run({system, user});
    check(r.values.walk.cycles == 3 && r.values.walk.size == 500,
          "config broken: a file that is not valid JSON sets nothing");
    check(contains(r.errors, "hd-pipes: " + user + ": "), "config broken: is reported");

    const auto list = config("list.json", R"({"hd-pipes": [1, 2]})");
    const Result l = run({system, list});
    check(l.values.walk.cycles == 3, "config broken: a key that is not an object sets nothing");
    check(contains(l.errors, list + ": hd-pipes is not an object, skipped"), "config broken: a key that is not an object is reported");
}

void test_command_line_wrong()
{
    struct Case {
        std::vector<std::string> args;
        const char* message;
    };
    for (const Case& c : std::vector<Case>{
             {{"--cycles", "0"}, "--cycles is a whole number from 1 to 10"},
             {{"--cycles", "11"}, "--cycles is a whole number from 1 to 10"},
             {{"--size=9"}, "--size is a whole number from 10 to 1000"},
             {{"--size", "1001"}, "--size is a whole number from 10 to 1000"},
             {{"--delay", "0"}, "--delay is a whole number from 1 to 1000000"},
             {{"--delay", "5x"}, "--delay is a whole number from 1 to 1000000"},
             {{"--joints", "teapot"}, "--joints is elbow, ball, bolted or cycle"},
             {{"--factory", "-1"}, "--factory is a whole number from 0 to 100"},
             {{"--factory", "101"}, "--factory is a whole number from 0 to 100"},
             {{"--framerate", "-1"}, "--framerate is a whole number from 0 to 1000"},
             {{"--framerate", "1001"}, "--framerate is a whole number from 0 to 1000"},
             {{"--msaa", "0"}, "--msaa is a whole number from 1 to 8"},
             {{"--msaa", "16"}, "--msaa is a whole number from 1 to 8"},
             {{"--cycles"}, "--cycles needs a value"},
             {{"--fisheye=yes"}, "--fisheye takes no value"},
             {{"--no-cycles"}, "unknown option --no-cycles"},
             {{"--no-window"}, "unknown option --no-window"},
             {{"--bolts"}, "unknown option --bolts"},
             {{"pipes"}, "unexpected argument pipes"},
         }) {
        const Result r = run({}, c.args);
        check(r.parsed == Options::Parsed::error, std::string("command line wrong: refuses for ") + c.message);
        check(contains(r.errors, std::string("hd-pipes: ") + c.message + "\n"),
              std::string("command line wrong: reports ") + c.message);
        check(contains(r.errors, "usage: hd-pipes"), std::string("command line wrong: the usage for ") + c.message);
    }
}

void test_help()
{
    const auto system = config("system.json", R"({"hd-pipes": {"cycles": 3}})");
    const Result r = run({system}, {"--help"});
    check(r.parsed == Options::Parsed::help, "help: --help");
    check(r.errors.empty(), "help: no messages");
    for (const char* line : {"usage: hd-pipes [options]", "--joints elbow|ball|bolted|cycle", "(cycle)",
                             "--cycles N", "1-10 (5)", "--size N", "10-1000 (500)", "--delay US", "(10000)",
                             "--fisheye, --no-fisheye", "--tightturns, --no-tightturns", "--framerate N",
                             "0-1000 (0)", "--msaa N", "1-8 (8)", "--factory N", "0-100 (2)", "--window"}) {
        check(contains(r.out, line), std::string("help: shows ") + line);
    }
}

} // namespace

int main()
{
    char name[] = "/tmp/test_options.XXXXXX";
    if (mkdtemp(name) == nullptr) {
        std::perror("mkdtemp");
        return 1;
    }
    directory = name;

    test_defaults();
    test_system();
    test_user_over_system();
    test_command_line();
    test_config_out_of_range();
    test_config_broken();
    test_command_line_wrong();
    test_help();

    std::filesystem::remove_all(directory);
    if (failures > 0) {
        std::printf("%d checks failed\n", failures);
        return 1;
    }
    std::printf("all checks passed\n");
    return 0;
}
