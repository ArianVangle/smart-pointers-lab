#include "Ui.hpp"

#include <cstdio>
#include <iostream>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace ui {
namespace {

constexpr std::size_t kMinFrameWidth = 62;

void FrameBorder(const char* left, const char* right, std::size_t width) {
    std::printf("  %s%s", Dim(), left);
    for (std::size_t i = 2; i < width; ++i) {
        std::printf("─");
    }
    std::printf("%s%s\n", right, Reset());
}

void FrameLine(const char* text, const char* color, std::size_t width) {
    std::printf("  %s│%s %s%s%s", Dim(), Reset(), color, text, Reset());
    for (std::size_t i = DisplayWidth(text) + 3; i < width; ++i) {
        std::printf(" ");
    }
    std::printf("%s│%s\n", Dim(), Reset());
}

std::size_t FrameWidth(const char* title, const char* subtitle) {
    std::size_t content = DisplayWidth(title);
    if (subtitle != nullptr && DisplayWidth(subtitle) > content) {
        content = DisplayWidth(subtitle);
    }
    const std::size_t needed = content + 4;
    return needed > kMinFrameWidth ? needed : kMinFrameWidth;
}

}  // namespace

bool Interactive() {
#if defined(_WIN32)
    static const bool value = _isatty(_fileno(stdin)) != 0 && _isatty(_fileno(stdout)) != 0;
#else
    static const bool value = isatty(STDIN_FILENO) != 0 && isatty(STDOUT_FILENO) != 0;
#endif
    return value;
}

const char* Reset() { return Interactive() ? "\033[0m" : ""; }
const char* Bold() { return Interactive() ? "\033[1m" : ""; }
const char* Dim() { return Interactive() ? "\033[2m" : ""; }
const char* Red() { return Interactive() ? "\033[31m" : ""; }
const char* Green() { return Interactive() ? "\033[32m" : ""; }
const char* Yellow() { return Interactive() ? "\033[33m" : ""; }
const char* Cyan() { return Interactive() ? "\033[36m" : ""; }
const char* Magenta() { return Interactive() ? "\033[35m" : ""; }

std::size_t DisplayWidth(const char* text) {
    std::size_t width = 0;
    for (const char* it = text; *it != '\0'; ++it) {
        if ((static_cast<unsigned char>(*it) & 0xC0) != 0x80) {
            ++width;
        }
    }
    return width;
}

void ClearScreen() {
    if (Interactive()) {
        std::printf("\033[2J\033[H");
        std::fflush(stdout);
    }
}

void BeginScreen(const char* title, const char* subtitle) {
    ClearScreen();
    const std::size_t width = FrameWidth(title, subtitle);
    FrameBorder("┌", "┐", width);
    FrameLine(title, Bold(), width);
    if (subtitle != nullptr) {
        FrameLine(subtitle, Dim(), width);
    }
    FrameBorder("└", "┘", width);
    std::printf("\n");
    std::fflush(stdout);
}

ScreenSession::ScreenSession() {
    if (Interactive()) {
        active_ = true;
        std::printf("\033[?1049h");
        std::fflush(stdout);
    }
}

ScreenSession::~ScreenSession() {
    if (active_) {
        std::printf("\033[?1049l");
        std::fflush(stdout);
    }
}

std::string AskLine(const char* question) {
    std::printf("%s%s%s ", Cyan(), question, Reset());
    std::fflush(stdout);
    std::string line;
    if (!std::getline(std::cin, line)) {
        return {};
    }
    return line;
}

void Pause(const char* text) {
    if (!Interactive()) {
        return;
    }
    std::printf("\n%s%s%s ", Dim(), text, Reset());
    std::fflush(stdout);
    std::string ignored;
    std::getline(std::cin, ignored);
}

}  // namespace ui
