// hd-message: the message box of the HyprDebian desktop, for Hyprland and
// other compositors with wlr-layer-shell. It reads the message as JSON from
// stdin (see message.hpp), shows its title, its text and its buttons, and
// prints the value of the button chosen on stdout.
//
//   echo '{"title": "Reboot system?", "text": "Unsaved work is lost.", "buttons": [{"label": "No"}, {"label": "Yes"}]}' | hd-message
//
// Left, Right, Tab, Shift+Tab, Home and End move the selection, Enter
// chooses and Escape or q cancels; held keys repeat. It exits with 0 after a
// choice, with 1 when the message is closed without one or another
// hd-message is already open, and with 2 on an error.
//
// The parts: message.cpp reads the message, renderer.cpp draws it with
// cairo, and the App here ties them together. The rest is in
// ../../common/src, shared with hd-menu: theme.cpp reads the hd-theme of the
// user, or of the system, icon.cpp loads the icons, draw.cpp draws the box,
// the text and the icons, and window.cpp puts the drawing on the screen and
// gets the keys.

#include "message.hpp"
#include "program.hpp"
#include "renderer.hpp"
#include "theme.hpp"
#include "window.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <iostream>
#include <linux/input-event-codes.h>
#include <string>
#include <sys/file.h>
#include <unistd.h>

const char* const PROGRAM = "hd-message";

namespace {

class App : public WindowHandler {
public:
    App(Message& message, const Renderer& renderer) : message_(message), renderer_(renderer) {}

    void set_window(Window* window) { window_ = window; }
    int result() const { return result_; }

    void draw(cairo_t* cr, int width, int height) override { renderer_.draw(cr, width, height, message_); }

    void key(uint32_t key, xkb_keysym_t sym) override
    {
        // q goes by the letter of the keyboard layout, with or without Shift
        // or Caps Lock.
        if (xkb_keysym_to_lower(sym) == XKB_KEY_q) {
            result_ = 1;
            window_->quit();
            return;
        }
        const int last = static_cast<int>(message_.buttons.size()) - 1;
        int selected = message_.selected;
        switch (key) {
        case KEY_ESC:
            result_ = 1;
            window_->quit();
            return;
        case KEY_ENTER:
        case KEY_KPENTER:
            std::cout << message_.buttons[message_.selected].value << std::endl;
            result_ = 0;
            window_->quit();
            return;
        case KEY_LEFT:
            selected -= 1;
            break;
        case KEY_RIGHT:
            selected += 1;
            break;
        case KEY_TAB:
            // Shift+Tab is Tab with the keysym ISO_Left_Tab.
            selected += sym == XKB_KEY_ISO_Left_Tab ? -1 : 1;
            break;
        case KEY_HOME:
            selected = 0;
            break;
        case KEY_END:
            selected = last;
            break;
        default:
            return;
        }
        message_.selected = std::clamp(selected, 0, last);
        window_->redraw();
    }

    void closed() override
    {
        result_ = 1;
        window_->quit();
    }

private:
    Message& message_;
    const Renderer& renderer_;
    Window* window_ = nullptr;
    int result_ = 1;
};

// Only one hd-message at a time: a lock in the runtime directory of the user
// in /run, which systemd-logind makes for each user with a session. The lock
// is held until hd-message exits, also when it crashes. The file descriptor
// is CLOEXEC, so that programs started from a script after hd-message do not
// hold it. Returns 0 with the lock, 1 when another hd-message holds it and 2
// when the lock cannot be taken.
int take_lock()
{
    const std::string path = "/run/user/" + std::to_string(getuid()) + "/hd-message.lock";
    const int fd = open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (fd < 0) {
        std::cerr << "hd-message: cannot open " << path << ": " << std::strerror(errno) << "\n";
        return 2;
    }
    if (flock(fd, LOCK_EX | LOCK_NB) == 0) {
        return 0;
    }
    if (errno == EWOULDBLOCK) {
        return 1;
    }
    std::cerr << "hd-message: cannot lock " << path << ": " << std::strerror(errno) << "\n";
    return 2;
}

} // namespace

int main(int argc, char**)
{
    if (argc > 1) {
        std::cerr << "usage: hd-message < message.json\n"
                     "hd-message takes no arguments; it reads the message as JSON from stdin:\n"
                     "  {\"title\": \"...\", \"text\": \"...\", \"width\": 600, \"buttons\": [{\"label\": \"...\", "
                     "\"value\": \"...\", \"icon\": \"/path/to.svg\"}, ...], \"default\": \"...\"}\n";
        return 2;
    }

    Message message;
    try {
        message = read_message(std::cin);
    } catch (const std::exception& e) {
        std::cerr << "hd-message: stdin: " << e.what() << "\n";
        return 2;
    }

    const int lock = take_lock();
    if (lock != 0) {
        return lock;
    }

    try {
        Renderer renderer(load_theme());
        App app(message, renderer);
        Window window(app);
        app.set_window(&window);
        window.run();
        return app.result();
    } catch (const std::exception& e) {
        std::cerr << "hd-message: " << e.what() << "\n";
        return 2;
    }
}
