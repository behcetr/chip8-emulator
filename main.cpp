// CHIP-8 emulator frontend (SDL2).
// Usage: ./chip8 <rom> [--modern] [--scale N] [--ipf N]
//   --modern   use modern (CHIP-48/SCHIP-style) quirks instead of the original CHIP-8 ones
//   (default = original CHIP-8: VF reset, memory I+=X+1, display wait, clipping, shift Vy, BNNN uses V0)
//   --scale N  window scale (default 12)
//   --ipf N    instructions per 60 Hz frame (default 12, ~720 IPS)

#include <SDL.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#include "chip8.hpp"

// Standard CHIP-8 keypad layout mapped onto the left side of a QWERTY keyboard:
//   1 2 3 C        1 2 3 4
//   4 5 6 D   ->   Q W E R
//   7 8 9 E        A S D F
//   A 0 B F        Z X C V
static int keyIndex(SDL_Keycode k) {
    switch (k) {
    case SDLK_1: return 0x1; case SDLK_2: return 0x2; case SDLK_3: return 0x3; case SDLK_4: return 0xC;
    case SDLK_q: return 0x4; case SDLK_w: return 0x5; case SDLK_e: return 0x6; case SDLK_r: return 0xD;
    case SDLK_a: return 0x7; case SDLK_s: return 0x8; case SDLK_d: return 0x9; case SDLK_f: return 0xE;
    case SDLK_z: return 0xA; case SDLK_x: return 0x0; case SDLK_c: return 0xB; case SDLK_v: return 0xF;
    default: return -1;
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <rom> [--modern] [--scale N] [--ipf N]\n", argv[0]);
        return 1;
    }
    Quirks quirks = Quirks::chip8();
    int scale = 12, ipf = 12;
    for (int i = 2; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--modern")) quirks = Quirks{};
        else if (!std::strcmp(argv[i], "--vip")) quirks = Quirks::chip8(); // kept for compatibility
        else if (!std::strcmp(argv[i], "--scale") && i + 1 < argc) scale = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--ipf") && i + 1 < argc) ipf = std::atoi(argv[++i]);
    }

    Chip8 chip(quirks);
    if (!chip.loadRom(argv[1])) {
        std::fprintf(stderr, "Could not load ROM: %s\n", argv[1]);
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window* win = SDL_CreateWindow("CHIP-8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       Chip8::WIDTH * scale, Chip8::HEIGHT * scale, 0);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
                                         Chip8::WIDTH, Chip8::HEIGHT);

    using clock = std::chrono::steady_clock;
    const auto frame = std::chrono::microseconds(16667); // 60 Hz
    auto next = clock::now();
    uint32_t pixels[Chip8::WIDTH * Chip8::HEIGHT];
    bool running = true;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = false;
                int k = keyIndex(e.key.keysym.sym);
                if (k >= 0) chip.setKey(k, e.type == SDL_KEYDOWN);
            }
        }

        for (int i = 0; i < ipf; ++i) chip.cycle();
        chip.tickTimers();

        if (chip.drawFlag) {
            const auto& g = chip.display();
            for (int i = 0; i < Chip8::WIDTH * Chip8::HEIGHT; ++i)
                pixels[i] = g[i] ? 0xFFFFFFFFu : 0x000000FFu;
            SDL_UpdateTexture(tex, nullptr, pixels, Chip8::WIDTH * sizeof(uint32_t));
            chip.drawFlag = false;
        }
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, nullptr, nullptr);
        SDL_RenderPresent(ren);

        next += frame;
        std::this_thread::sleep_until(next);
    }

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
