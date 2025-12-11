# NES Emulator

A cycle-accurate Nintendo Entertainment System (NES) emulator written in C++20.

## Features

- Cycle-accurate 6502 CPU with all official and illegal opcodes
- Cycle-accurate PPU (Picture Processing Unit) with sprite 0 hit detection
- APU (Audio Processing Unit) with all 5 channels and high-quality filtering
- 13 mapper support covering ~95% of NES games
- SDL2-based frontend with audio and video
- FPS counter and resizable window

## Supported Mappers

| Mapper | Name | Example Games |
|--------|------|---------------|
| 0 | NROM | Super Mario Bros, Donkey Kong |
| 1 | MMC1 | Legend of Zelda, Metroid, Mega Man 2 |
| 2 | UxROM | Mega Man, Castlevania, Contra |
| 3 | CNROM | Gradius, Arkanoid |
| 4 | MMC3 | Super Mario Bros 3, Kirby's Adventure |
| 7 | AxROM | Battletoads, Marble Madness |
| 9 | MMC2 | Punch-Out!! |
| 10 | MMC4 | Fire Emblem |
| 11 | Color Dreams | Bible Adventures |
| 66 | GxROM | Super Mario Bros + Duck Hunt |
| 71 | Camerica | Micro Machines, Fire Hawk |
| 206 | Namco 108 | Gauntlet, Pac-Land |

## Building

### Prerequisites

- CMake 3.20 or higher
- C++20 compatible compiler (GCC 10+, Clang 12+, or MSVC 2019+)
- SDL2 development libraries

### macOS

```bash
# Install dependencies using Homebrew
brew install sdl2 cmake

# Clone or navigate to the project directory
cd nes-emulator

# Create build directory and compile
mkdir -p build
cd build
cmake ..
make -j4

# The executable will be at ./nes_emulator
```

### Linux (Ubuntu/Debian)

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install build-essential cmake libsdl2-dev

# Clone or navigate to the project directory
cd nes-emulator

# Create build directory and compile
mkdir -p build
cd build
cmake ..
make -j4

# The executable will be at ./nes_emulator
```

### Linux (Fedora/RHEL)

```bash
# Install dependencies
sudo dnf install gcc-c++ cmake SDL2-devel

# Clone or navigate to the project directory
cd nes-emulator

# Create build directory and compile
mkdir -p build
cd build
cmake ..
make -j4
```

### Windows

```bash
# Install SDL2 from https://www.libsdl.org/download-2.0.php
# Extract to a known location (e.g., C:\SDL2)

# Open Developer Command Prompt or PowerShell
cd nes-emulator

mkdir build
cd build
cmake .. -DSDL2_DIR=C:\SDL2
cmake --build . --config Release

# The executable will be at .\Release\nes_emulator.exe
```

## Running the Emulator

### Basic Usage

```bash
./nes_emulator <path-to-rom-file>
```

### Examples

```bash
# macOS/Linux
./nes_emulator ~/Downloads/SuperMarioBros.nes
./nes_emulator '/path/to/game with spaces.nes'

# Windows
nes_emulator.exe C:\ROMs\SuperMarioBros.nes
```

### Supported ROM Formats

- `.nes` - iNES format (most common)
- `.nes` - NES 2.0 format (extended metadata)

The emulator will display ROM information when loading:
```
Loading ROM: /path/to/game.nes
File size: 40976 bytes
PRG ROM: 2 x 16KB = 32KB
CHR ROM: 1 x 8KB = 8KB
Mapper: 0
Mirroring: Vertical
Using Mapper 0 (NROM) - No bank switching
  Games: Donkey Kong, Super Mario Bros
ROM loaded successfully. Mapper: 0
```

## Controls

### Keyboard Layout

| NES Button | Primary Key | Alternative |
|------------|-------------|-------------|
| D-Pad Up | ↑ (Arrow) | W |
| D-Pad Down | ↓ (Arrow) | S |
| D-Pad Left | ← (Arrow) | A |
| D-Pad Right | → (Arrow) | D |
| A Button | X | K |
| B Button | Z | J |
| Start | E | Enter / Space |
| Select | Q | Shift |

### System Controls

| Action | Key |
|--------|-----|
| Reset Game | Ctrl + R |
| Quit Emulator | ESC |

### Control Tips

- Use **Arrow Keys + X/Z** for classic arcade-style controls
- Use **WASD + X/Z** for modern keyboard layout
- **E** for Start works well with WASD layout (no conflicts)
- The window can be resized while maintaining aspect ratio

## Troubleshooting

### ROM Won't Load

1. **Check file path**: Make sure the path is correct
   ```bash
   # Use quotes for paths with spaces
   ./nes_emulator '/Users/name/My ROMs/game.nes'
   ```

2. **Check ROM format**: The emulator supports iNES (.nes) format
   - File should start with "NES" header (bytes: 4E 45 53 1A)

3. **Check mapper support**: Some games use unsupported mappers
   - The emulator will show "Unsupported mapper X" if not supported

### No Audio

1. Make sure SDL2 audio is working on your system
2. Check system volume
3. Some games have quiet audio - this is normal

### Game Runs Too Fast/Slow

- The emulator targets 60 FPS (NTSC timing)
- Check the FPS counter in the window title
- Close other applications if performance is poor

### Graphics Glitches

- Some games may have minor graphical issues
- Games using unsupported mappers will have major issues
- Try a different ROM dump if problems persist

## Technical Information

### CPU
- MOS 6502 processor running at 1.789773 MHz (NTSC)
- All 151 official opcodes implemented
- 105 illegal/undocumented opcodes implemented
- Cycle-accurate timing

### PPU
- Resolution: 256x240 pixels
- 60 Hz refresh rate (NTSC)
- 262 scanlines per frame
- Sprite 0 hit detection for split-screen effects
- 8 sprites per scanline limit

### APU
- 2 Pulse wave channels
- 1 Triangle wave channel
- 1 Noise channel
- 1 DMC (Delta Modulation) channel
- High-quality audio filtering

### Memory Map
```
$0000-$07FF  2KB Internal RAM
$2000-$2007  PPU Registers
$4000-$4017  APU and I/O Registers
$4020-$FFFF  Cartridge Space (ROM/RAM)
```

## File Structure

```
nes-emulator/
├── include/           # Header files
│   ├── cpu.hpp
│   ├── ppu.hpp
│   ├── apu.hpp
│   ├── bus.hpp
│   ├── cartridge.hpp
│   ├── mapper.hpp
│   └── mappers/       # Mapper headers
├── src/               # Source files
│   ├── main.cpp
│   ├── cpu.cpp
│   ├── ppu.cpp
│   ├── apu.cpp
│   ├── bus.cpp
│   ├── cartridge.cpp
│   ├── emulator.cpp
│   ├── frontend.cpp
│   └── mappers/       # Mapper implementations
├── build/             # Build output (created by cmake)
├── CMakeLists.txt     # Build configuration
└── README.md          # This file
```

## License

GNU License

## Credits

- NESdev Wiki (https://www.nesdev.org/) for hardware documentation
- SDL2 library for cross-platform audio/video
