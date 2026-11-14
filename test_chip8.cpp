// Headless unit tests for the CHIP-8 core. Build: make test && ./test_chip8
#include <cstdio>
#include <vector>

#include "chip8.hpp"

static int failures = 0;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("  FAIL line %d: %s\n", __LINE__, #cond);            \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static void run(Chip8& c, const std::vector<uint8_t>& prog, int cycles) {
    c.loadBytes(prog);
    for (int i = 0; i < cycles; ++i) c.cycle();
}

static void test_load_and_add() {
    std::printf("load/add\n");
    Chip8 c;
    run(c, {0x60, 0x0A, 0x61, 0x05, 0x70, 0x03}, 3); // V0=10, V1=5, V0+=3
    CHECK(c.V[0] == 13);
    CHECK(c.V[1] == 5);
}

static void test_add_carry() {
    std::printf("8XY4 carry\n");
    Chip8 c;
    run(c, {0x60, 0xFF, 0x61, 0x02, 0x80, 0x14}, 3); // 255 + 2
    CHECK(c.V[0] == 1);
    CHECK(c.V[0xF] == 1);
}

static void test_sub_borrow() {
    std::printf("8XY5 borrow flag\n");
    Chip8 c;
    run(c, {0x60, 0x05, 0x61, 0x0A, 0x80, 0x15}, 3); // 5 - 10 underflows
    CHECK(c.V[0] == 251);
    CHECK(c.V[0xF] == 0);
    run(c, {0x60, 0x0A, 0x61, 0x05, 0x80, 0x15}, 3); // 10 - 5
    CHECK(c.V[0] == 5);
    CHECK(c.V[0xF] == 1);
}

static void test_flag_register_as_operand() {
    std::printf("VF as destination keeps the flag\n");
    Chip8 c;
    run(c, {0x6F, 0xFF, 0x61, 0x02, 0x8F, 0x14}, 3); // VF=255; VF += 2 -> flag overwrites result
    CHECK(c.V[0xF] == 1);
}

static void test_shift() {
    std::printf("shifts\n");
    Chip8 c;
    run(c, {0x60, 0x05, 0x80, 0x06}, 2); // 5 >> 1
    CHECK(c.V[0] == 2);
    CHECK(c.V[0xF] == 1);
    run(c, {0x60, 0x81, 0x80, 0x0E}, 2); // 0x81 << 1
    CHECK(c.V[0] == 0x02);
    CHECK(c.V[0xF] == 1);
}

static void test_call_return() {
    std::printf("call/return\n");
    Chip8 c;
    // 0x200: CALL 0x206 ; 0x202: LD V1,7 ; 0x204: JP 0x204 ; 0x206: LD V0,9 ; 0x208: RET
    run(c, {0x22, 0x06, 0x61, 0x07, 0x12, 0x04, 0x60, 0x09, 0x00, 0xEE}, 3); // CALL, LD V0, RET
    CHECK(c.V[0] == 9);
    CHECK(c.pc == 0x202);
    c.cycle();
    CHECK(c.V[1] == 7);
}

static void test_skip() {
    std::printf("skips\n");
    Chip8 c;
    run(c, {0x60, 0x07, 0x30, 0x07, 0x61, 0x01, 0x62, 0x02}, 3); // SE skips LD V1
    CHECK(c.V[1] == 0);
    CHECK(c.V[2] == 2);
}

static void test_bcd() {
    std::printf("FX33 BCD\n");
    Chip8 c;
    run(c, {0x60, 0xFB, 0xA3, 0x00, 0xF0, 0x33}, 3); // 251
    CHECK(c.mem[0x300] == 2);
    CHECK(c.mem[0x301] == 5);
    CHECK(c.mem[0x302] == 1);
}

static void test_store_load() {
    std::printf("FX55/FX65\n");
    Chip8 c;
    run(c, {0x60, 0x11, 0x61, 0x22, 0x62, 0x33, 0xA3, 0x00, 0xF2, 0x55}, 5);
    CHECK(c.mem[0x300] == 0x11);
    CHECK(c.mem[0x301] == 0x22);
    CHECK(c.mem[0x302] == 0x33);
    CHECK(c.I == 0x300); // modern behaviour: I unchanged
    Chip8 v({true, true, false});
    run(v, {0x60, 0x11, 0x61, 0x22, 0x62, 0x33, 0xA3, 0x00, 0xF2, 0x55}, 5);
    CHECK(v.I == 0x303); // VIP behaviour: I += X + 1
}

static void test_draw_and_collision() {
    std::printf("DXYN draw + collision\n");
    Chip8 c;
    // V0=0 (digit), I = font for V0, draw at (0,0) height 5, then draw again.
    run(c, {0x60, 0x00, 0xF0, 0x29, 0xD1, 0x15}, 3);
    const auto& g = c.display();
    CHECK(g[0] == 1 && g[1] == 1 && g[2] == 1 && g[3] == 1); // top row of "0" is 0xF0
    CHECK(g[4] == 0);
    CHECK(c.V[0xF] == 0);
    c.pc = Chip8::START_ADDR + 4;
    c.cycle(); // draw again -> erases and reports collision
    CHECK(c.V[0xF] == 1);
    CHECK(g[0] == 0);
}

static void test_draw_clipping() {
    std::printf("sprite clips at screen edge\n");
    Chip8 c;
    run(c, {0x60, 0x3E, 0x61, 0x00, 0xF0, 0x29, 0xD0, 0x15}, 4); // x=62, 0xF0 row
    const auto& g = c.display();
    CHECK(g[62] == 1 && g[63] == 1);
    CHECK(g[0] == 0); // must not wrap to the left side
}

static void test_clear() {
    std::printf("00E0 clear\n");
    Chip8 c;
    run(c, {0xF0, 0x29, 0xD0, 0x05, 0x00, 0xE0}, 3);
    bool any = false;
    for (auto p : c.display()) any |= p;
    CHECK(!any);
}

static void test_key_wait() {
    std::printf("FX0A waits for key\n");
    Chip8 c;
    c.loadBytes({0xF3, 0x0A});
    c.cycle();
    CHECK(c.pc == Chip8::START_ADDR); // still waiting
    c.setKey(0xA, true);
    c.cycle();
    CHECK(c.V[3] == 0xA);
    CHECK(c.pc == Chip8::START_ADDR + 2);
}

static void test_timers() {
    std::printf("timers\n");
    Chip8 c;
    run(c, {0x60, 0x03, 0xF0, 0x15, 0xF0, 0x18}, 3);
    CHECK(c.soundOn());
    c.tickTimers(); c.tickTimers(); c.tickTimers();
    CHECK(!c.soundOn());
}

int main() {
    test_load_and_add();
    test_add_carry();
    test_sub_borrow();
    test_flag_register_as_operand();
    test_shift();
    test_call_return();
    test_skip();
    test_bcd();
    test_store_load();
    test_draw_and_collision();
    test_draw_clipping();
    test_clear();
    test_key_wait();
    test_timers();
    if (failures) {
        std::printf("\n%d check(s) FAILED\n", failures);
        return 1;
    }
    std::printf("\nAll tests passed\n");
    return 0;
}
