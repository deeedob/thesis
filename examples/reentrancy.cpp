// reentrancy.cpp
#include <iostream>

struct Plugin
{
    Plugin(int num) { std::cout << "Plugin " << num << " created." << std::endl; }
};

Plugin create_reentrant(int n) {
    std::cout << "Reentrant Call " << n << ", ";
    return Plugin(n);
}

Plugin create_non_reentrant(int n) {
    std::cout << "Non-Reentrant Call " << n << ", ";
    static Plugin plugin(n);
    return plugin;
}

int main() {
    create_reentrant(1);
    create_non_reentrant(1);
    std::cout << std::endl;
    create_reentrant(2);
    create_non_reentrant(2);
    return 0;
}
