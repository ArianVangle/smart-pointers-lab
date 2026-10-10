#pragma once

#include <cstddef>
#include <string>

namespace ui {

bool Interactive();

const char* Reset();
const char* Bold();
const char* Dim();
const char* Red();
const char* Green();
const char* Yellow();
const char* Cyan();
const char* Magenta();

std::size_t DisplayWidth(const char* text);

void ClearScreen();

void BeginScreen(const char* title, const char* subtitle = nullptr);

class ScreenSession {
  public:
    ScreenSession();
    ~ScreenSession();

    ScreenSession(const ScreenSession&) = delete;
    ScreenSession& operator=(const ScreenSession&) = delete;

  private:
    bool active_ = false;
};

std::string AskLine(const char* question);

void Pause(const char* text = "Enter — дальше");

}  // namespace ui
