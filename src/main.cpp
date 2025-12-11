#include "frontend.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <rom_file.nes>" << std::endl;
        return 1;
    }
    
    Frontend frontend;
    
    if (!frontend.init()) {
        std::cerr << "Failed to initialize frontend" << std::endl;
        return 1;
    }
    
    frontend.run(argv[1]);
    
    return 0;
}
