#include <iostream>
#include <cstdint>
#include <raylib.h>
#include "cpu.hpp"

#define CLOCK_SPEED 1000

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

termios originalTerminal;

void init_terminal() {
    tcgetattr(STDIN_FILENO, &originalTerminal);

    termios terminal = originalTerminal;

    terminal.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &terminal);

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

void reset_terminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &originalTerminal);

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
}


Color RGBI2222ToRaylib(uint8_t value) {
    int r = (value >> 6) & 0x03;
    int g = (value >> 4) & 0x03;
    int b = (value >> 2) & 0x03;
    int i =  value       & 0x03;

    r *= 85;
    g *= 85;
    b *= 85;

    float intensity = i / 3.0f;

    return Color{(unsigned char)(r * intensity),(unsigned char)(g * intensity),(unsigned char)(b * intensity),255};
}

Color framebufferPixels[512 * 384];
Texture2D framebufferTexture;

void updateFramebuffer(Cpu &c) {
    if (c.vga->mode == 0) {
        for (int i = 0; i < 512 * 384; i++)
            framebufferPixels[i] = RGBI2222ToRaylib(c.vga->vram[i]);

        UpdateTexture(framebufferTexture, framebufferPixels);
        DrawTexture(framebufferTexture, 0, 0, WHITE);
    } else {
        ClearBackground(BLACK);

        for (int y = 0; y < 38; y++) {
            for (int x = 0; x < 51; x++) {
                uint16_t cell = c.vga->text_vram[y * 128 + x];
                uint8_t ch = cell & 0xFF;
                uint8_t color = cell >> 8;

                if (ch)
                    DrawText(TextFormat("%c", ch), x * 10, y * 10, 10, RGBI2222ToRaylib(color));
            }
        }
    }
}

uint64_t cycles;
uint64_t totalCycles;

void updateText(Cpu &cpu) {
    DrawRectangle(512, 0, 256, 1024, DARKBLUE);

    char text[64];

    sprintf(text, "A:  %04X", cpu.a);
    DrawText(text, 520, 20, 20, WHITE);

    sprintf(text, "B:  %04X", cpu.b);
    DrawText(text, 520, 45, 20, WHITE);

    sprintf(text, "XA: %08X", cpu.xa);
    DrawText(text, 520, 70, 20, WHITE);

    sprintf(text, "XB: %08X", cpu.xb);
    DrawText(text, 520, 95, 20, WHITE);

    sprintf(text, "DS: %04X", cpu.ds);
    DrawText(text, 520, 130, 20, WHITE);

    sprintf(text, "CS: %04X", cpu.cs);
    DrawText(text, 520, 155, 20, WHITE);

    sprintf(text, "ES: %04X", cpu.es);
    DrawText(text, 520, 180, 20, WHITE);

    sprintf(text, "SP: %08X", cpu.sp);
    DrawText(text, 520, 215, 20, WHITE);

    sprintf(text, "IP: %08X", cpu.ip);
    DrawText(text, 520, 240, 20, WHITE);

    sprintf(text, "CLOCK: %.2fkHz",
            (float)CLOCK_SPEED / 1000);
    DrawText(text, 520, 265, 20, WHITE);

    DrawText(TextFormat("FPS: %d", GetFPS()), 520, 300, 20, WHITE);

    sprintf(text, "CYCLES: %llu", totalCycles);
    DrawText(text, 520, 325, 20, WHITE);
    if (cpu.halted) {
        DrawText("(HALTED)", 520, 364, 20, WHITE);
    }
}

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    init_terminal();
    InitWindow(768, 384, "Emulator");
    SetTraceLogLevel(LOG_NONE);
    SetTargetFPS(240);
    Cpu cpu;
    cpu.loadBinary(std::string(argv[1]));
    Image framebufferImage = GenImageColor(512, 384, BLACK);
    framebufferTexture = LoadTextureFromImage(framebufferImage);
    UnloadImage(framebufferImage);
    while(!WindowShouldClose()) {
        BeginDrawing();
            ClearBackground(BLACK);
            int fps = GetFPS();
            if (fps == 0) fps++;
            for (int i = 0; i < CLOCK_SPEED / fps; i++) {
                if (!cpu.halted) totalCycles++;
                cycles++;
                if (cycles >= CLOCK_SPEED / 1000) {
                    cycles = 0;
                    if (cpu.halted && cpu.ports[0xF0]->provideOutput() && cpu.ports[0xFF]->provideOutput()) {
                        if(cpu.ports[0xF1]->provideOutput()) {
                            cpu.ports[0xF1]->acceptInput(cpu.ports[0xF1]->provideOutput() - 1);
                        } else {
                            cpu.halted = false;
                        }
                    }
                }
                if (!cpu.halted) cpu.step();
            }
            updateFramebuffer(cpu);
            updateText(cpu);
            cpu.serial.update();
        EndDrawing();
    }
    CloseWindow();
    reset_terminal();
    return 0;
}
