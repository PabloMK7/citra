#pragma once

// Fix glad.h including windows.h
#ifdef _WIN32
#include "citra_qt/wayland/windows_headers.h"
#endif

#include <glad/glad.h>
