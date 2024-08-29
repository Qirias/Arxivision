#include "source/engine_pch.hpp"

#include "source/app.h"

// std
#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main() {
    arx::App app{};
    
    try {
        app.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
