#include "config.h"

namespace ripple_demo {

// Isolated consumer of config.h (mirrors settings.cpp including it too).
const char* configVersion() { return "1.0.0"; }

}  // namespace ripple_demo
