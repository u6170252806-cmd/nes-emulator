cmake_minimum_required(VERSION 3.20)
project(NESEmulator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find SDL2
find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
    # Try using sdl2-config
    execute_process(
        COMMAND sdl2-config --cflags
        OUTPUT_VARIABLE SDL2_CFLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND sdl2-config --libs
        OUTPUT_VARIABLE SDL2_LIBRARIES
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    string(REGEX REPLACE "^-I" "" SDL2_INCLUDE_DIRS "${SDL2_CFLAGS}")
    string(REGEX REPLACE " -I" ";" SDL2_INCLUDE_DIRS "${SDL2_INCLUDE_DIRS}")
    string(REGEX REPLACE "^-L" "" SDL2_LIBRARIES "${SDL2_LIBRARIES}")
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/cpu.cpp
    src/ppu.cpp
    src/apu.cpp
    src/bus.cpp
    src/cartridge.cpp
    src/mapper.cpp
    src/emulator.cpp
    src/frontend.cpp
    src/mappers/mapper000.cpp
    src/mappers/mapper001.cpp
    src/mappers/mapper002.cpp
    src/mappers/mapper003.cpp
    src/mappers/mapper004.cpp
    src/mappers/mapper007.cpp
    src/mappers/mapper009.cpp
    src/mappers/mapper010.cpp
    src/mappers/mapper011.cpp
    src/mappers/mapper066.cpp
    src/mappers/mapper071.cpp
    src/mappers/mapper206.cpp
)

# Create executable
add_executable(nes_emulator ${SOURCES})

# Include directories
target_include_directories(nes_emulator PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SDL2_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(nes_emulator PRIVATE ${SDL2_LIBRARIES})

# Compiler warnings
if(MSVC)
    target_compile_options(nes_emulator PRIVATE /W4)
else()
    target_compile_options(nes_emulator PRIVATE 
        -Wall -Wextra 
        -Wno-gnu-anonymous-struct
        -Wno-nested-anon-types
        -Wno-unused-parameter
    )
endif()
