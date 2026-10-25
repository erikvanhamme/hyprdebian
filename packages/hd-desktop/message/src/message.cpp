#include "message.hpp"

#include "json.hpp"

#include <iterator>
#include <stdexcept>

namespace {

Button read_button(JsonReader& reader)
{
    Button button;
    std::string icon;
    reader.object([&](const std::string& key) {
        if (key == "label") {
            button.label = reader.string();
        } else if (key == "value") {
            button.value = reader.string();
        } else if (key == "icon") {
            icon = reader.string();
        } else {
            reader.skip();
        }
    });
    if (button.label.empty()) {
        throw std::runtime_error("a button has no label");
    }
    if (button.value.empty()) {
        button.value = button.label;
    }
    if (!icon.empty()) {
        button.icon = load_icon(icon);
    }
    return button;
}

} // namespace

Message read_message(std::istream& in)
{
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    JsonReader reader(text);
    Message message;
    bool has_default = false;
    std::string default_value;
    reader.object([&](const std::string& key) {
        if (key == "title") {
            message.title = reader.string();
        } else if (key == "text") {
            message.text = reader.string();
        } else if (key == "width") {
            message.width = read_size(reader, key);
        } else if (key == "buttons") {
            reader.array([&] { message.buttons.push_back(read_button(reader)); });
        } else if (key == "default") {
            has_default = true;
            default_value = reader.string();
        } else {
            reader.skip();
        }
    });
    reader.end();
    if (message.buttons.empty()) {
        throw std::runtime_error("there are no buttons");
    }
    if (has_default) {
        bool found = false;
        for (std::size_t i = 0; i < message.buttons.size(); ++i) {
            if (message.buttons[i].value == default_value) {
                message.selected = static_cast<int>(i);
                found = true;
                break;
            }
        }
        if (!found) {
            throw std::runtime_error("default \"" + default_value + "\" is not the value of a button");
        }
    }
    return message;
}
