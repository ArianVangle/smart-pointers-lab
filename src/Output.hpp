#pragma once

namespace output {

bool Open(const char* path);
void Close();
bool IsOpen();

void Printf(const char* format, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;

}  // namespace output
