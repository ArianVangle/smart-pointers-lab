#include "Output.hpp"

#include <cstdarg>
#include <cstdio>
#include <filesystem>

namespace output {
namespace {

std::FILE* g_file = nullptr;

}  // namespace

bool Open(const char* path) {
    Close();
    if (path == nullptr) {
        return false;
    }
    std::error_code error;
    const std::filesystem::path parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
    }
    g_file = std::fopen(path, "w");
    return g_file != nullptr;
}

void Close() {
    if (g_file != nullptr) {
        std::fclose(g_file);
        g_file = nullptr;
    }
}

bool IsOpen() { return g_file != nullptr; }

void Printf(const char* format, ...) {
    char buffer[4096];
    std::va_list arguments;
    va_start(arguments, format);
    const int written = std::vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    if (written < 0) {
        return;
    }
    std::fputs(buffer, stdout);
    std::fflush(stdout);
    if (g_file != nullptr) {
        std::fputs(buffer, g_file);
    }
}

}  // namespace output
