#ifndef RIPPLE_DEMO_SETTINGS_H
#define RIPPLE_DEMO_SETTINGS_H

#include "config.h"  // deliberate chain: config.h -> settings.h -> main.cpp

namespace ripple_demo {

/// Two-level dependency for the demo chain:
///   config.h  ->  settings.h  ->  main.cpp
int themeDepth();

}  // namespace ripple_demo

#endif
