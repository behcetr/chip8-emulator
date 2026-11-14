#include "chip8.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace {
const uint8_t FONTSET[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};
} // namespace

Chip8::Chip8(Quirks q) : quirks_(q) { reset(); }

void Chip8::reset() {
    std::memset(mem, 0, sizeof(mem));
    std::memset(V, 0, sizeof(V));
    std::memset(stack_, 0, sizeof(stack_));
    std::memset(keys_, 0, sizeof(keys_));
    gfx_.fill(0);
    I = 0;
    pc = START_ADDR;
    sp_ = 0;
    delayTimer_ = soundTimer_ = 0;
    vblank_ = true;
    drawFlag = false;
    std::memcpy(mem + FONT_ADDR, FONTSET, sizeof(FONTSET));
}

bool Chip8::loadRom(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    std::streamsize size = f.tellg();
    if (size <= 0 || size > 4096 - START_ADDR) return false;
    f.seekg(0);
    reset();
    f.read(reinterpret_cast<char*>(mem + START_ADDR), size);
    return static_cast<bool>(f);
}

void Chip8::loadBytes(const std::vector<uint8_t>& program) {
    reset();
    std::copy(program.begin(), program.end(), mem + START_ADDR);
}

void Chip8::tickTimers() {
    vblank_ = true;
    if (delayTimer_ > 0) --delayTimer_;
    if (soundTimer_ > 0) --soundTimer_;
}

void Chip8::cycle() {
    // Fetch (big-endian 16-bit opcode).
    uint16_t op = (mem[pc & 0xFFF] << 8) | mem[(pc + 1) & 0xFFF];
    pc += 2;

    // Decode the common fields.
    uint8_t  x   = (op >> 8) & 0xF;
    uint8_t  y   = (op >> 4) & 0xF;
    uint8_t  n   = op & 0xF;
    uint8_t  nn  = op & 0xFF;
    uint16_t nnn = op & 0xFFF;

    switch (op >> 12) {
    case 0x0:
        if (op == 0x00E0) {                 // CLS
            gfx_.fill(0);
            drawFlag = true;
        } else if (op == 0x00EE) {          // RET
            if (sp_ > 0) pc = stack_[--sp_];
        }
        break;
    case 0x1: pc = nnn; break;              // JP nnn
    case 0x2:                               // CALL nnn
        if (sp_ < 16) stack_[sp_++] = pc;
        pc = nnn;
        break;
    case 0x3: if (V[x] == nn) pc += 2; break;       // SE Vx, nn
    case 0x4: if (V[x] != nn) pc += 2; break;       // SNE Vx, nn
    case 0x5: if (n == 0 && V[x] == V[y]) pc += 2; break; // SE Vx, Vy
    case 0x6: V[x] = nn; break;                     // LD Vx, nn
    case 0x7: V[x] += nn; break;                    // ADD Vx, nn (no flag)
    case 0x8:
        switch (n) {
        case 0x0: V[x] = V[y]; break;
        case 0x1: V[x] |= V[y]; V[0xF] = 0; break;  // VF reset is original behaviour
        case 0x2: V[x] &= V[y]; V[0xF] = 0; break;
        case 0x3: V[x] ^= V[y]; V[0xF] = 0; break;
        case 0x4: {                                  // ADD with carry
            uint16_t sum = V[x] + V[y];
            V[x] = sum & 0xFF;
            V[0xF] = sum > 0xFF;                     // flag written last
            break;
        }
        case 0x5: {                                  // SUB Vx = Vx - Vy
            bool noBorrow = V[x] >= V[y];
            V[x] = V[x] - V[y];
            V[0xF] = noBorrow;
            break;
        }
        case 0x6: {                                  // SHR
            uint8_t src = quirks_.shiftUsesVy ? V[y] : V[x];
            V[x] = src >> 1;
            V[0xF] = src & 1;
            break;
        }
        case 0x7: {                                  // SUBN Vx = Vy - Vx
            bool noBorrow = V[y] >= V[x];
            V[x] = V[y] - V[x];
            V[0xF] = noBorrow;
            break;
        }
        case 0xE: {                                  // SHL
            uint8_t src = quirks_.shiftUsesVy ? V[y] : V[x];
            V[x] = src << 1;
            V[0xF] = (src >> 7) & 1;
            break;
        }
        }
        break;
    case 0x9: if (n == 0 && V[x] != V[y]) pc += 2; break; // SNE Vx, Vy
    case 0xA: I = nnn; break;                       // LD I, nnn
    case 0xB: pc = nnn + (quirks_.jumpUsesVx ? V[x] : V[0]); break;
    case 0xC: V[x] = (rng_() & 0xFF) & nn; break;   // RND
    case 0xD: {                                     // DRW Vx, Vy, n
        if (quirks_.displayWait) {
            if (!vblank_) { pc -= 2; break; }       // stall until the next 60 Hz frame
            vblank_ = false;
        }
        uint8_t px = V[x] % WIDTH;
        uint8_t py = V[y] % HEIGHT;
        V[0xF] = 0;
        for (int row = 0; row < n; ++row) {
            if (py + row >= HEIGHT) break;          // clip at the bottom edge
            uint8_t sprite = mem[(I + row) & 0xFFF];
            for (int bit = 0; bit < 8; ++bit) {
                if (px + bit >= WIDTH) break;       // clip at the right edge
                if (sprite & (0x80 >> bit)) {
                    uint8_t& pixel = gfx_[(py + row) * WIDTH + (px + bit)];
                    if (pixel) V[0xF] = 1;          // collision
                    pixel ^= 1;
                }
            }
        }
        drawFlag = true;
        break;
    }
    case 0xE:
        if (nn == 0x9E) { if (keys_[V[x] & 0xF]) pc += 2; }       // SKP
        else if (nn == 0xA1) { if (!keys_[V[x] & 0xF]) pc += 2; } // SKNP
        break;
    case 0xF:
        switch (nn) {
        case 0x07: V[x] = delayTimer_; break;
        case 0x0A: {                                 // wait for a key press
            bool pressed = false;
            for (int k = 0; k < 16; ++k)
                if (keys_[k]) { V[x] = k; pressed = true; break; }
            if (!pressed) pc -= 2;                   // repeat this instruction
            break;
        }
        case 0x15: delayTimer_ = V[x]; break;
        case 0x18: soundTimer_ = V[x]; break;
        case 0x1E: I += V[x]; break;
        case 0x29: I = FONT_ADDR + (V[x] & 0xF) * 5; break;
        case 0x33:                                   // BCD
            mem[I & 0xFFF] = V[x] / 100;
            mem[(I + 1) & 0xFFF] = (V[x] / 10) % 10;
            mem[(I + 2) & 0xFFF] = V[x] % 10;
            break;
        case 0x55:
            for (int i = 0; i <= x; ++i) mem[(I + i) & 0xFFF] = V[i];
            if (quirks_.loadStoreIncI) I += x + 1;
            break;
        case 0x65:
            for (int i = 0; i <= x; ++i) V[i] = mem[(I + i) & 0xFFF];
            if (quirks_.loadStoreIncI) I += x + 1;
            break;
        }
        break;
    }
}
