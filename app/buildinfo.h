#pragma once

// App qmake builds generate this header in their build directory. Standalone
// test targets keep their existing supplied macros or the defaults below.
#if __has_include("starlight_generated_buildinfo.h")
#include "starlight_generated_buildinfo.h"
#endif

#ifndef STARLIGHT_BUILD_REVISION
#define STARLIGHT_BUILD_REVISION "unknown"
#endif
#ifndef STARLIGHT_VERSION_STR
#define STARLIGHT_VERSION_STR "development"
#endif
#ifndef VERSION_STR
#define VERSION_STR "unknown"
#endif
