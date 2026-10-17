#include "config.h"

namespace algo1_demo {

// Isolated consumer of config.h (mirrors settings.cpp including it too).
const char* configVersion() { return "1.0.0"; }

}  // namespace algo1_demo
