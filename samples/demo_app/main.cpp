#include "home.h"
#include "settings.h"

#include <iostream>
#include <string>

int main() {
    std::cout << "demo-app booting...\n";
    const int visits = algo1_demo::homeVisits(3);
    const int depth = algo1_demo::themeDepth();
    std::cout << "visits=" << visits << " theme-depth=" << depth << "\n";
    std::cout << "demo-app done\n";
    return 0;
}
