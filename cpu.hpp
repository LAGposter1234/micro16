#pragma once
#define LOADCODE 0xC00000
#include <cstdint>
#include <vector>
#include <fstream>
#include <string>
#include <fcntl.h>
#include <unistd.h>

typedef enum {
    INCA = 0,
    INCB = 1,
    SETA = 2,
    SETB = 3,
    SDS = 4, // set DS
    SCS = 5, // set CS
    SES = 6, // set ES
    JMP = 8,
    OUT = 9, // in/out ports
    IN = 10,
    // ad and substract
    ADAAB = 11,
    ADABB = 12,

    SBAAB = 13,
    SBABB = 14,
    SBBAB = 15,

    // swaps
    SWPAB = 16,
    SWPADS = 17,
    SWPACS = 18,
    SWPAES = 19,

    FJMPA = 21, // IP = ES << 8 + B?

    PUSHA = 22,
    PUSHB = 23,
    POPA = 24,
    POPB = 25,

    JAZ = 27,
    JBZ = 28,

    LDA = 29,
    LDB = 30,

    SRA = 31,
    SRB = 32,

    LDESA = 33,
    LDESB = 34,

    SRESA = 35,
    SRESB = 36,

    OUTA = 37,
    OUTB = 38,

    DECA = 39,
    DECB = 40,

    SETXA = 41,
    SETXB = 42,
    INCXA = 43,
    INCXB = 44,
    DECXA = 45,
    DECXB = 46,

    LDXA  = 47,
    LDXB  = 48,
    SRXA  = 49,
    SRXB  = 50,

    SWPXAXB = 51,
    SWPXAAB = 52, // swap XA-> A, B
    SWPXBAB = 53,

    INCC = 54,
    SETC = 55,
    DECC = 56,

    LDC = 57, // LDC 0x1234
    LDAC = 58, // SETA 0x1234 LDAC
    LDBC = 59,
    SRC = 60, // SETC 0x12 SRC 0x1234
    SRAC = 61, // SETA 0x1234 SETC 0x12 SRAC
    SRBC = 62,

    OUTC = 63, // most likely used for serial
    INC = 64,

    JCZ = 65,

    INT = 66,
    IRET = 67,

    HLT = 255
} opcode_t;

class Port {
public:
    uint8_t port;

    virtual void acceptInput(uint16_t value) = 0;
    virtual uint16_t provideOutput() = 0;
};

class RegisterPort : public Port {
public:
    uint16_t val = 0;
    void acceptInput(uint16_t value) override {
        val = value;
    }
    uint16_t provideOutput() override {
        return val;
    }
};

class SerialPort : public Port {
public:
    bool inputReady = false;
    uint8_t input = 0;
    void acceptInput(uint16_t value) override {
        std::cout << static_cast<char>(value) << std::flush;
    }

    void update() {
        char c;

        if (read(STDIN_FILENO, &c, 1) > 0) {
            input = (uint8_t)c;
            inputReady = true;
        }
    }

    uint16_t provideOutput() override {
        if (!inputReady)
            return 0;

        inputReady = false;
        uint8_t value = input;
        input = 0;
        return (uint8_t)value;
    }
};

class VgaPort : public Port {
public:
    uint16_t &x;
    uint16_t &y;
    uint8_t mode = 0;
    uint8_t vram[512 * 384] = {};
    uint16_t text_vram[51 * 38] = {};

    VgaPort(uint16_t &x, uint16_t &y)
    : x(x), y(y) {}

    void acceptInput(uint16_t value) override {
        if (value == 0x80) {
            mode = x;
            return;
        }

        if (mode == 0) {
            if (x >= 512 || y >= 384)
                return;
            vram[y * 512 + x] = value;
        } else {
            if (x >= 51 || y >= 38)
                return;
            text_vram[y * 51 + x] = value;
        }
    }

    uint16_t provideOutput() override {
        if (mode == 0)
            return vram[y * 512 + x];
        return text_vram[y * 51 + x];
    }
};

// to get the interrupt vector to call, its fault + 0xF0
typedef enum {
    FAULT_GENERAL_PROTECTION = 0, // this would be 0xF0
    FAULT_INVALID_CS = 1,
    FAULT_INVALID_FJMP = 2,
    FAULT_INVALID_INTERRUPT_VECTOR = 3,
    FAULT_DOUBLE_FAULT = 0xF
} Fault;

class Cpu {
public:
    Cpu() {
        a = 0;
        b = 0;
        xa = 0;
        xb = 0;
        ds = 0;
        cs = 0xC0; // 0x00C0:0000
        es = 0;
        ip = (uint32_t)cs << 16;

        sp = 0xA000;
        halted = false;

        serial.port = 0x10;
        ports[0x10] = &serial;

        vga = new VgaPort(a, b);
        vga->port = 0x20;
        ports[0x20] = vga;

        waking.port = 0xFF;
        ports[0xFF] = &waking;

        timerPeriod.port = 0xF1;
        ports[0xF1] = &timerPeriod;

        timerEnable.port = 0xF0;
        ports[0xF0] = &timerEnable;
    }
    void loadBinary(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);

        if (!file) {
            throw std::runtime_error("Failed to open binary");
        }

        uint32_t address = (uint32_t)cs << 16;

        char byte;
        while (file.get(byte)) {
            if (address >= ram.size()) {
                throw std::runtime_error("Binary is too large for RAM");
            }

            ram[address++] = static_cast<uint8_t>(byte);
        }
    }
    RegisterPort timerEnable;
    RegisterPort timerPeriod;
    RegisterPort waking;
    std::vector<uint8_t> ram = std::vector<uint8_t>(0x100000000);
    std::vector<Port*> ports = std::vector<Port*>(256);
    SerialPort serial;
    VgaPort* vga;
    uint16_t a; // registers
    uint16_t b;
    uint32_t xa;
    uint32_t xb;
    uint16_t ds; // segment registers
    uint16_t cs;
    uint16_t es;

    uint32_t sp;

    uint32_t ip;

    uint8_t c;

    bool halted;
    void out(uint8_t port, uint16_t value) {
        if (ports[port]) {
            ports[port]->acceptInput(value);
        } else {
            throw std::runtime_error("Port " + std::to_string(port) + "not connected to anything.");
        }
    }
    void in(uint8_t port) {
        if (ports[port]) {
            a = ports[port]->provideOutput();
        } else {
            throw std::runtime_error("Port " + std::to_string(port) + "not connected to anything.");
        }
    }
    void push16(uint16_t topush) {
        ram[sp] = topush & 0xFF;
        ram[sp + 1] = (topush >> 8) & 0xFF;
        sp += 2;
    }
    uint16_t pop16() {
        sp -= 2;
        return (uint16_t)ram[sp] | ((uint16_t)ram[sp + 1] << 8);
    }
    void push32(uint32_t topush) {
        ram[sp] = topush & 0xFF;
        ram[sp + 1] = (topush >> 8) & 0xFF;
        ram[sp + 2] = (topush >> 16) & 0xFF;
        ram[sp + 3] = (topush >> 24) & 0xFF;
        sp += 4;
    }

    uint32_t pop32() {
        sp -= 4;
        return (uint32_t)ram[sp]
        | ((uint32_t)ram[sp + 1] << 8)
        | ((uint32_t)ram[sp + 2] << 16)
        | ((uint32_t)ram[sp + 3] << 24);
    }
    void interrupt(uint8_t num) {
        uint32_t address = num * 4;
        uint16_t newip = (uint16_t)ram[address] | ((uint16_t)ram[address + 1] << 8);
        uint16_t newcs = (uint16_t)ram[address + 2] | ((uint16_t)ram[address + 3] << 8);
        uint32_t physical = ((uint32_t)newcs << 16) + newip;
        std::cout << "Software interrupt going to 0x" << std::hex << physical << std::endl;
        if (physical >= ram.size() || physical < (0x0010 << 16) + 0x0000) { // valid interrupt range = 0xFFFF:FFFF - 0x0010:0000
            if (num == FAULT_INVALID_INTERRUPT_VECTOR + 0xF0) raise_fault(FAULT_DOUBLE_FAULT);
            else raise_fault(FAULT_INVALID_INTERRUPT_VECTOR);
            return;
        }

        push16(cs);
        push32(ip + 2);

        ip = physical;
        cs = newcs;

    }
    void interrupt_return(void) {
        ip = pop32();
        cs = pop16();
    }
    void raise_fault(uint8_t faultindex) {
        faultindex = faultindex & 0x0F; // ensure 4 bit
        if (faultindex == FAULT_DOUBLE_FAULT) halted = true;
        interrupt(faultindex + 0xF0);
    }

    void step() {
        if (halted) return;
        uint8_t opcode = ram[ip];
        switch (opcode) {
            case INCA:
                a++;
                ip++;
                break;

            case INCB:
                b++;
                ip++;
                break;

            case SETA:
                a = ram[ip + 1] | ((uint16_t)ram[ip + 2] << 8);
                ip += 3;
                break;

            case SETB:
                b = ram[ip + 1] | ((uint16_t)ram[ip + 2] << 8);
                ip += 3;
                break;

            case SDS:
                ds = ram[ip + 1] | ((uint16_t)ram[ip + 2] << 8);
                ip += 3;
                break;

            case SCS:
                cs = ram[ip + 1] | ((uint16_t)ram[ip + 2] << 8);
                ip += 3;
                break;

            case SES:
                es = ram[ip + 1] | ((uint16_t)ram[ip + 2] << 8);
                ip += 3;
                break;

            case JMP:
                ip = ram[ip + 1] |
                ((uint32_t)ram[ip + 2] << 8) |
                ((uint32_t)ram[ip + 3] << 16) |
                ((uint32_t)ram[ip + 4] << 24);
                break;

            case OUT:
                out(ram[ip + 1], ram[ip + 3] | (ram[ip + 4] << 8));
                ip += 5;
                break;

            case IN:
                in(ram[ip+1]);
                ip+=2;
                break;


                // Arithmetic

            case ADAAB:
                b = a + a;
                ip++;
                break;

            case ADABB:
                b = a + b;
                ip++;
                break;

            case SBAAB:
                b = a - a;
                ip++;
                break;

            case SBABB:
                b = a - b;
                ip++;
                break;

            case SBBAB:
                b = b - a;
                ip++;
                break;


                // Swaps

            case SWPAB:
                std::swap(a, b);
                ip++;
                break;

            case SWPADS:
                std::swap(a, ds);
                ip++;
                break;

            case SWPACS:
                std::swap(a, cs);
                ip++;
                break;

            case SWPAES:
                std::swap(a, es);
                ip++;
                break;


                // IP = ES << 16 + B

            case FJMPA:
                ip = ((uint32_t)es << 16) | b;
                break;


                // Stack

            case PUSHA:
                ram[sp] = a & 0xFF;
                ram[sp + 1] = (a >> 8) & 0xFF;
                sp += 2;
                ip++;
                break;

            case PUSHB:
                ram[sp] = b & 0xFF;
                ram[sp + 1] = (b >> 8) & 0xFF;
                sp += 2;
                ip++;
                break;

            case POPA:
                sp -= 2;
                a = ram[sp] | ((uint16_t)ram[sp + 1] << 8);
                ip++;
                break;

            case POPB:
                sp -= 2;
                b = ram[sp] | ((uint16_t)ram[sp + 1] << 8);
                ip++;
                break;


                // Conditional jumps

            case JAZ:
                if (a == 0)
                    ip = ram[ip + 1] |
                    ((uint32_t)ram[ip + 2] << 8) |
                    ((uint32_t)ram[ip + 3] << 16) |
                    ((uint32_t)ram[ip + 4] << 24);
                else
                    ip += 5;
            break;

            case JBZ:
                if (b == 0)
                    ip = ram[ip + 1] |
                    ((uint32_t)ram[ip + 2] << 8) |
                    ((uint32_t)ram[ip + 3] << 16) |
                    ((uint32_t)ram[ip + 4] << 24);
                else
                    ip += 5;
            break;


            // DS memory

            case LDA: {
                uint16_t address = ram[ip + 1] |
                ((uint16_t)ram[ip + 2] << 8);

                uint32_t full_address = ((uint32_t)ds << 16) | address;

                a = ram[full_address] |
                ((uint16_t)ram[full_address + 1] << 8);

                ip += 3;
                break;
            }

            case LDB: {
                uint16_t address = ram[ip + 1] |
                ((uint16_t)ram[ip + 2] << 8);

                uint32_t full_address = ((uint32_t)ds << 16) | address;

                b = ram[full_address] |
                ((uint16_t)ram[full_address + 1] << 8);

                ip += 3;
                break;
            }

            case SRA: {
                uint16_t address = ram[ip + 1] |
                ((uint16_t)ram[ip + 2] << 8);

                uint32_t full_address = ((uint32_t)ds << 16) | address;

                ram[full_address] = a & 0xFF;
                ram[full_address + 1] = a >> 8;

                ip += 3;
                break;
            }

            case SRB: {
                uint16_t address = ram[ip + 1] |
                ((uint16_t)ram[ip + 2] << 8);

                uint32_t full_address = ((uint32_t)ds << 16) | address;

                ram[full_address] = b & 0xFF;
                ram[full_address + 1] = b >> 8;

                ip += 3;
                break;
            }


            // ES memory

            case LDESA: {
                uint16_t address = ram[ip + 1] |
                ((uint16_t)ram[ip + 2] << 8);

                uint32_t full_address = ((uint32_t)es << 16) | address;

                a = ram[full_address] |
                ((uint16_t)ram[full_address + 1] << 8);

                ip += 3;
                break;
            }

            case LDESB: {
                uint16_t address = ram[ip + 1] |
                ((uint16_t)ram[ip + 2] << 8);

                uint32_t full_address = ((uint32_t)es << 16) | address;

                b = ram[full_address] |
                ((uint16_t)ram[full_address + 1] << 8);

                ip += 3;
                break;
            }

            case SRESA: {
                uint16_t address = ram[ip + 1] |
                ((uint16_t)ram[ip + 2] << 8);

                uint32_t full_address = ((uint32_t)es << 16) | address;

                ram[full_address] = a & 0xFF;
                ram[full_address + 1] = a >> 8;

                ip += 3;
                break;
            }

            case SRESB: {
                uint16_t address = ram[ip + 1] |
                ((uint16_t)ram[ip + 2] << 8);

                uint32_t full_address = ((uint32_t)es << 16) | address;

                ram[full_address] = b & 0xFF;
                ram[full_address + 1] = b >> 8;

                ip += 3;
                break;
            }

            case OUTA:
                out(ram[ip+1], a);
                ip += 2;
                break;

            case OUTB:
                out(ram[ip+1], b);
                ip += 2;
                break;

            case SETXA:
                xa = ram[ip + 1] |
                ((uint32_t)ram[ip + 2] << 8) |
                ((uint32_t)ram[ip + 3] << 16) |
                ((uint32_t)ram[ip + 4] << 24);
                ip += 5;
                break;

            case SETXB:
                xb = ram[ip + 1] |
                ((uint32_t)ram[ip + 2] << 8) |
                ((uint32_t)ram[ip + 3] << 16) |
                ((uint32_t)ram[ip + 4] << 24);
                ip += 5;
                break;

            case INCXA:
                xa++;
                ip++;
                break;

            case INCXB:
                xb++;
                ip++;
                break;

            case DECXA:
                xa--;
                ip++;
                break;

            case DECXB:
                xb--;
                ip++;
                break;

            case LDXA: {
                uint32_t address = ram[ip + 1] |
                ((uint32_t)ram[ip + 2] << 8) |
                ((uint32_t)ram[ip + 3] << 16) |
                ((uint32_t)ram[ip + 4] << 24);

                xa = ram[address] |
                ((uint32_t)ram[address + 1] << 8) |
                ((uint32_t)ram[address + 2] << 16) |
                ((uint32_t)ram[address + 3] << 24);

                ip += 5;
                break;
            }

            case LDXB: {
                uint32_t address = ram[ip + 1] |
                ((uint32_t)ram[ip + 2] << 8) |
                ((uint32_t)ram[ip + 3] << 16) |
                ((uint32_t)ram[ip + 4] << 24);

                xb = ram[address] |
                ((uint32_t)ram[address + 1] << 8) |
                ((uint32_t)ram[address + 2] << 16) |
                ((uint32_t)ram[address + 3] << 24);

                ip += 5;
                break;
            }

            case SRXA: {
                uint32_t address = ram[ip + 1] |
                ((uint32_t)ram[ip + 2] << 8) |
                ((uint32_t)ram[ip + 3] << 16) |
                ((uint32_t)ram[ip + 4] << 24);

                ram[address]     = xa & 0xFF;
                ram[address + 1] = (xa >> 8) & 0xFF;
                ram[address + 2] = (xa >> 16) & 0xFF;
                ram[address + 3] = (xa >> 24) & 0xFF;

                ip += 5;
                break;
            }

            case SRXB: {
                uint32_t address = ram[ip + 1] |
                ((uint32_t)ram[ip + 2] << 8) |
                ((uint32_t)ram[ip + 3] << 16) |
                ((uint32_t)ram[ip + 4] << 24);

                ram[address]     = xb & 0xFF;
                ram[address + 1] = (xb >> 8) & 0xFF;
                ram[address + 2] = (xb >> 16) & 0xFF;
                ram[address + 3] = (xb >> 24) & 0xFF;

                ip += 5;
                break;
            }

            case SWPXAXB: {
                uint32_t temp = xa;
                xa = xb;
                xb = temp;

                ip++;
                break;
            }

            case SWPXAAB: {
                uint32_t temp = xa;

                xa = (uint32_t)a | ((uint32_t)b << 16);

                a = temp & 0xFFFF;
                b = (temp >> 16) & 0xFFFF;

                ip++;
                break;
            }

            case SWPXBAB: {
                uint32_t temp = xb;

                xb = (uint32_t)a | ((uint32_t)b << 16);

                a = temp & 0xFFFF;
                b = (temp >> 16) & 0xFFFF;

                ip++;
                break;
            }

            case DECA:
                a--;
                ip++;
                break;

            case DECB:
                b--;
                ip++;
                break;

            case LDC: {
                uint16_t address = ram[ip + 1] |
                ((uint16_t)ram[ip + 2] << 8);

                uint32_t full_address = ((uint32_t)ds << 16) | address;

                c = ram[full_address];

                ip += 3;
                break;
            }

            case LDAC: {
                uint32_t full_address = ((uint32_t)ds << 16) | a;

                c = ram[full_address];

                ip++;
                break;
            }

            case LDBC: {
                uint32_t full_address = ((uint32_t)ds << 16) | b;

                c = ram[full_address];

                ip++;
                break;
            }

            case SRC: {
                uint16_t address = ram[ip + 1] |
                ((uint16_t)ram[ip + 2] << 8);

                uint32_t full_address = ((uint32_t)ds << 16) | address;

                ram[full_address] = c;

                ip += 3;
                break;
            }

            case SRAC: {
                uint32_t full_address = ((uint32_t)ds << 16) | a;

                ram[full_address] = c;

                ip++;
                break;
            }

            case SRBC: {
                uint32_t full_address = ((uint32_t)ds << 16) | b;

                ram[full_address] = c;

                ip++;
                break;
            }

            case OUTC:
                out(ram[ip + 1], c);
                ip += 2;
                break;

            case INC:
                if (ports[ram[ip + 1]]) {
                    c = ports[ram[ip + 1]]->provideOutput();
                } else {
                    throw std::runtime_error("Port " + std::to_string(ram[ip + 1]) + "not connected to anything.");
                }
                ip += 2;
                break;

            case JCZ:
                if (c == 0)
                    ip = ram[ip + 1] |
                    ((uint32_t)ram[ip + 2] << 8) |
                    ((uint32_t)ram[ip + 3] << 16) |
                    ((uint32_t)ram[ip + 4] << 24);
                else
                    ip += 5;
            break;

            case INT: {
                    uint8_t num = ram[ip + 1];
                    interrupt(num);
                    break;
            }

            case IRET:
                    interrupt_return();
            break;

            case HLT:
                halted = true;
                ip++;
                break;

            default:
                // Unknown opcode
                halted = true;
                break;
        }
    }
};
