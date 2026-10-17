#ifndef ALGO1_DEMO_SETTINGS_H
#define ALGO1_DEMO_SETTINGS_H

#include "config.h"  // deliberate chain: config.h -> settings.h -> main.cpp

namespace algo1_demo {

/// Two-level dependency for the demo chain:
///   config.h  ->  settings.h  ->  main.cpp
int themeDepth();

}  // namespace algo1_demo

#endif
