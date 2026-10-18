/**
 * @file json.hpp
 * @brief Just enough JSON for hd-menu.
 *
 * A reader that walks through the text, reads strings, numbers and booleans,
 * and skips whatever the caller does not want, without building a tree.
 */
#pragma once

#include <cstddef>
#include <functional>
#include <string>

/**
 * @brief Reads JSON text value by value, from the start to the end.
 *
 * The caller asks for the type it expects next; each method reads one value
 * and moves past it. Every method throws std::runtime_error, with the offset
 * in the text, when the text is not what it expects.
 */
class JsonReader {
public:
    /**
     * @brief Starts reading at the beginning of text.
     * @param text The JSON text.
     */
    explicit JsonReader(std::string text) : s_(std::move(text)) {}

    /**
     * @brief Looks at the next value without reading it.
     * @return The first character of the next value, after white space, or
     *         '\0' at the end of the text.
     */
    char peek();

    /**
     * @brief Reads an object.
     * @param on_member Called with the key of each member, in order. It must
     *                  read or skip the value of the member.
     * @throws std::runtime_error When the next value is not an object.
     */
    void object(const std::function<void(const std::string&)>& on_member);

    /**
     * @brief Reads an array.
     * @param on_element Called for each element, in order. It must read or
     *                   skip the element.
     * @throws std::runtime_error When the next value is not an array.
     */
    void array(const std::function<void()>& on_element);

    /**
     * @brief Reads a string.
     *
     * The escapes \\", \\\\, \\/, \\b, \\f, \\n, \\r, \\t and \\uXXXX are
     * decoded; \\uXXXX as UTF-8, for code points of the Basic Multilingual
     * Plane only.
     *
     * @return The string, in UTF-8.
     * @throws std::runtime_error When the next value is not a string.
     */
    std::string string();

    /**
     * @brief Reads a number.
     * @return The number.
     * @throws std::runtime_error When the next value is not a number.
     */
    double number();

    /**
     * @brief Reads true or false.
     * @return The boolean.
     * @throws std::runtime_error When the next value is not a boolean.
     */
    bool boolean();

    /**
     * @brief Skips the next value, of any type, with all it contains.
     * @throws std::runtime_error When the next value is not valid JSON.
     */
    void skip();

    /**
     * @brief Checks that the text has been read to the end.
     * @throws std::runtime_error When more than white space is left.
     */
    void end();

private:
    /**
     * @brief Moves past the character c, after white space.
     * @param c The character that must come next.
     * @throws std::runtime_error When another character comes next.
     */
    void expect(char c);

    /**
     * @brief Throws the error what, with the current offset.
     * @param what What is wrong.
     * @throws std::runtime_error Always.
     */
    [[noreturn]] void fail(const std::string& what) const;

    /** @brief The JSON text. */
    std::string s_;
    /** @brief The offset in s_ of the next character to read. */
    std::size_t i_ = 0;
};
