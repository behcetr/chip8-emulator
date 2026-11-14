#pragma once
#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

// Behaviours that differ between CHIP-8 interpreters over the years.
struct Quirks {
    bool shiftUsesVy = false;   // 8XY6/8XYE: true = shift Vy (COSMAC VIP), false = shift Vx (CHIP-48+)
    bool loadStoreIncI = false; // FX55/FX65: true = I += X+1 afterwards (COSMAC VIP)
    bool jumpUsesVx = false;    // BNNN: true = jump to NNN + Vx (CHIP-48), false = NNN + V0
    bool displayWait = false;   // DXYN: true = at most one sprite draw per 60 Hz frame (COSMAC VIP vblank wait)

    // Original CHIP-8 (COSMAC VIP) behaviour, as expected by the Timendus quirks test for "CHIP-8".
    static Quirks chip8() { return {true, true, false, true}; }
};

class Chip8 {
public:
    static constexpr int WIDTH = 64;
    static constexpr int HEIGHT = 32;
    static constexpr uint16_t START_ADDR = 0x200;
    static constexpr uint16_t FONT_ADDR = 0x50;

    explicit Chip8(Quirks q = {});

    bool loadRom(const std::string& path);
    void loadBytes(const std::vector<uint8_t>& program); // used by the unit tests

    void cycle();        // fetch, decode, execute one instruction
    void tickTimers();   // call at 60 Hz

    void setKey(int key, bool pressed) { keys_[key & 0xF] = pressed; }
    const std::array<uint8_t, WIDTH * HEIGHT>& display() const { return gfx_; }
    bool soundOn() const { return soundTimer_ > 0; }
    bool drawFlag = false;

    // Exposed for tests and debugging.
    uint8_t V[16]{};
    uint16_t I = 0;
    uint16_t pc = START_ADDR;
    uint8_t mem[4096]{};

private:
    void reset();

    uint16_t stack_[16]{};
    uint8_t sp_ = 0;
    uint8_t delayTimer_ = 0;
    uint8_t soundTimer_ = 0;
    bool vblank_ = true; // set every 60 Hz tick, consumed by DXYN when displayWait is on
    std::array<uint8_t, WIDTH * HEIGHT> gfx_{};
    bool keys_[16]{};
    Quirks quirks_;
    std::mt19937 rng_{std::random_device{}()};
};
