#include "json.hpp"

#include <cstdlib>
#include <stdexcept>

char JsonReader::peek()
{
    while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' || s_[i_] == '\r')) {
        ++i_;
    }
    return i_ < s_.size() ? s_[i_] : '\0';
}

void JsonReader::expect(char c)
{
    if (peek() != c) {
        fail(std::string("expected '") + c + "'");
    }
    ++i_;
}

void JsonReader::fail(const std::string& what) const
{
    throw std::runtime_error(what + " at offset " + std::to_string(i_));
}

void JsonReader::object(const std::function<void(const std::string&)>& on_member)
{
    expect('{');
    if (peek() == '}') {
        ++i_;
        return;
    }
    while (true) {
        const std::string key = string();
        expect(':');
        on_member(key);
        if (peek() != ',') {
            break;
        }
        ++i_;
    }
    expect('}');
}

void JsonReader::array(const std::function<void()>& on_element)
{
    expect('[');
    if (peek() == ']') {
        ++i_;
        return;
    }
    while (true) {
        on_element();
        if (peek() != ',') {
            break;
        }
        ++i_;
    }
    expect(']');
}

std::string JsonReader::string()
{
    expect('"');
    std::string out;
    while (i_ < s_.size() && s_[i_] != '"') {
        char c = s_[i_++];
        if (c == '\\' && i_ < s_.size()) {
            c = s_[i_++];
            switch (c) {
            case 'n': c = '\n'; break;
            case 't': c = '\t'; break;
            case 'r': c = '\r'; break;
            case 'b': c = '\b'; break;
            case 'f': c = '\f'; break;
            case 'u': {
                // A code point of the Basic Multilingual Plane, as UTF-8.
                const unsigned long cp = std::strtoul(s_.substr(i_, 4).c_str(), nullptr, 16);
                i_ += 4;
                if (cp < 0x80) {
                    out += static_cast<char>(cp);
                } else if (cp < 0x800) {
                    out += static_cast<char>(0xC0 | (cp >> 6));
                    out += static_cast<char>(0x80 | (cp & 0x3F));
                } else {
                    out += static_cast<char>(0xE0 | (cp >> 12));
                    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                    out += static_cast<char>(0x80 | (cp & 0x3F));
                }
                continue;
            }
            default: break;  // \" \\ \/
            }
        }
        out += c;
    }
    expect('"');
    return out;
}

double JsonReader::number()
{
    peek();
    const char* begin = s_.c_str() + i_;
    char* end = nullptr;
    const double value = std::strtod(begin, &end);
    if (end == begin) {
        fail("expected a number");
    }
    i_ += end - begin;
    return value;
}

bool JsonReader::boolean()
{
    peek();
    if (s_.compare(i_, 4, "true") == 0) {
        i_ += 4;
        return true;
    }
    if (s_.compare(i_, 5, "false") == 0) {
        i_ += 5;
        return false;
    }
    fail("expected true or false");
}

void JsonReader::skip()
{
    const char c = peek();
    if (c == '{') {
        object([this](const std::string&) { skip(); });
    } else if (c == '[') {
        array([this] { skip(); });
    } else if (c == '"') {
        string();
    } else if (c == 't' || c == 'f') {
        boolean();
    } else if (s_.compare(i_, 4, "null") == 0) {
        i_ += 4;
    } else {
        number();
    }
}

void JsonReader::end()
{
    if (peek() != '\0') {
        fail("unexpected text after the JSON");
    }
}
