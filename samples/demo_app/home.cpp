#include "home.h"

#include <iostream>

#include "user_model.h"

namespace ripple_demo {

// Deliberate dependency: this .cpp includes user_model.h directly, so
// editing user_model.h must flag home.cpp as affected.
int homeVisits(const int initial) {
    ripple_demo::UserModel user("demo-user");
    for (int i = 0; i < initial; ++i) user.addVisit();
    std::cout << "[home] visits=" << user.visits() << "\n";
    return user.visits();
}

}  // namespace ripple_demo
