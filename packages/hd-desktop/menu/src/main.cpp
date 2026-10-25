// hd-menu: the keyboard-driven menu of the HyprDebian desktop, for Hyprland
// and other compositors with wlr-layer-shell. It reads the menu as JSON from stdin (see menu.hpp)
// and prints the value of the option chosen on stdout.
//
//   echo '{"banner": "Power off system?", "options": [{"label": "No"}, {"label": "Yes"}]}' | hd-menu
//
// The arrows, Tab, Page Up, Page Down, Home and End move the selection,
// Enter chooses and Escape or q cancels; held keys repeat. It exits with 0
// after a choice, with 1 when the menu is closed without one or another
// hd-menu is already open, and with 2 on an error.
//
// The parts: menu.cpp reads the menu, renderer.cpp draws it with cairo, and
// the App here ties them together. The rest is in ../../common/src, shared
// with hd-message: theme.cpp reads the hd-theme of the user, or of the
// system, icon.cpp loads the icons, draw.cpp draws the box, the text and the
// icons, and window.cpp puts the drawing on the screen and gets the keys.

#include "menu.hpp"
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

const char* const PROGRAM = "hd-menu";

namespace {

class App : public WindowHandler {
public:
    App(Menu& menu, const Renderer& renderer) : menu_(menu), renderer_(renderer) {}

    void set_window(Window* window) { window_ = window; }
    int result() const { return result_; }

    void draw(cairo_t* cr, int width, int height) override { renderer_.draw(cr, width, height, menu_); }

    void key(uint32_t key, xkb_keysym_t sym) override
    {
        // q goes by the letter of the keyboard layout, with or without Shift
        // or Caps Lock.
        if (xkb_keysym_to_lower(sym) == XKB_KEY_q) {
            result_ = 1;
            window_->quit();
            return;
        }
        const int last = static_cast<int>(menu_.options.size()) - 1;
        int selected = menu_.selected;
        switch (key) {
        case KEY_ESC:
            result_ = 1;
            window_->quit();
            return;
        case KEY_ENTER:
        case KEY_KPENTER:
            std::cout << menu_.options[menu_.selected].value << std::endl;
            result_ = 0;
            window_->quit();
            return;
        case KEY_UP:
            selected -= 1;
            break;
        case KEY_DOWN:
        case KEY_TAB:
            selected += 1;
            break;
        case KEY_PAGEUP:
            selected -= renderer_.rows();
            break;
        case KEY_PAGEDOWN:
            selected += renderer_.rows();
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
        menu_.selected = std::clamp(selected, 0, last);
        window_->redraw();
    }

    void closed() override
    {
        result_ = 1;
        window_->quit();
    }

private:
    Menu& menu_;
    const Renderer& renderer_;
    Window* window_ = nullptr;
    int result_ = 1;
};

// Only one hd-menu at a time: a lock in the runtime directory of the user in
// /run, which systemd-logind makes for each user with a session. The lock is
// held until hd-menu exits, also when it crashes. The file descriptor is
// CLOEXEC, so that programs started from a script after hd-menu do not hold
// it. Returns 0 with the lock, 1 when another hd-menu holds it and 2 when
// the lock cannot be taken.
int take_lock()
{
    const std::string path = "/run/user/" + std::to_string(getuid()) + "/hd-menu.lock";
    const int fd = open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (fd < 0) {
        std::cerr << "hd-menu: cannot open " << path << ": " << std::strerror(errno) << "\n";
        return 2;
    }
    if (flock(fd, LOCK_EX | LOCK_NB) == 0) {
        return 0;
    }
    if (errno == EWOULDBLOCK) {
        return 1;
    }
    std::cerr << "hd-menu: cannot lock " << path << ": " << std::strerror(errno) << "\n";
    return 2;
}

} // namespace

int main(int argc, char**)
{
    if (argc > 1) {
        std::cerr << "usage: hd-menu < menu.json\n"
                     "hd-menu takes no arguments; it reads the menu as JSON from stdin:\n"
                     "  {\"banner\": \"...\", \"width\": 400, \"height\": \"30%\", \"options\": [{\"label\": \"...\", \"value\": \"...\", "
                     "\"icon\": \"/path/to.png\", \"default\": true}, ...]}\n";
        return 2;
    }

    Menu menu;
    try {
        menu = read_menu(std::cin);
    } catch (const std::exception& e) {
        std::cerr << "hd-menu: stdin: " << e.what() << "\n";
        return 2;
    }

    const int lock = take_lock();
    if (lock != 0) {
        return lock;
    }

    try {
        Renderer renderer(load_theme());
        App app(menu, renderer);
        Window window(app);
        app.set_window(&window);
        window.run();
        return app.result();
    } catch (const std::exception& e) {
        std::cerr << "hd-menu: " << e.what() << "\n";
        return 2;
    }
}
