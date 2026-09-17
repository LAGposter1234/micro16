import sys


CODE_START = 0xC00000

OPCODES = {
    "INCA": 0,
    "INCB": 1,
    "SETA": 2,
    "SETB": 3,
    "SDS": 4,
    "SCS": 5,
    "SES": 6,

    "JMP": 8,
    "OUT": 9,
    "IN": 10,

    "ADAAB": 11,
    "ADABB": 12,
    "SBAAB": 13,
    "SBABB": 14,
    "SBBAB": 15,

    "SWPAB": 16,
    "SWPADS": 17,
    "SWPACS": 18,
    "SWPAES": 19,

    "FJMPA": 21,

    "PUSHA": 22,
    "PUSHB": 23,
    "POPA": 24,
    "POPB": 25,

    "JAZ": 27,
    "JBZ": 28,

    "LDA": 29,
    "LDB": 30,

    "SRA": 31,
    "SRB": 32,

    "LDESA": 33,
    "LDESB": 34,

    "SRESA": 35,
    "SRESB": 36,

    "OUTA": 37,
    "OUTB": 38,

    "DECA": 39,
    "DECB": 40,

    "SETXA": 41,
    "SETXB": 42,
    "INCXA": 43,
    "INCXB": 44,
    "DECXA": 45,
    "DECXB": 46,

    "LDXA": 47,
    "LDXB": 48,
    "SRXA": 49,
    "SRXB": 50,

    "SWPXAXB": 51,
    "SWPXAAB": 52,
    "SWPXBAB": 53,

    "INCC": 54,
    "SETC": 55,
    "DECC": 56,
    "LDC": 57,
    "LDAC": 58,
    "LDBC": 59,
    "SRC": 60,
    "SRAC": 61,
    "SRBC": 62,
    "OUTC": 63,
    "INC": 64,
    "JCZ": 65,

    "INT": 66,
    "IRET": 67,

    # EZMEM
    "STRA": 68,
    "STRB": 69,
    "LODA": 70,
    "LODB": 71,
    "STRIMMIMM": 72,
    "STRAIMM": 73,
    "STRIMMA": 74,
    "STRAB": 75,

    # FUNCY

    "CALL": 76,
    "RET": 77,

    "HLT": 255,
}


ONE_BYTE_OPERAND = {
    "IN",
    "OUTA",
    "OUTB",
    "INC",
    "OUTC",
    "INT",
    "SETC",
}


TWO_BYTE_OPERAND = {
    "SETA",
    "SETB",
    "SDS",
    "SCS",
    "SES",

    "LDA",
    "LDB",
    "SRA",
    "SRB",

    "LDESA",
    "LDESB",
    "SRESA",
    "SRESB",

    "LDC",
    "SRC",

    # EZMEM
    "STRA",
    "STRB",
    "LODA",
    "LODB",
    "STRAIMM",
    "STRIMMA",
}


FOUR_BYTE_OPERAND = {
    "SETXA",
    "SETXB",

    "LDXA",
    "LDXB",
    "SRXA",
    "SRXB",
    "CALL",
}


NO_OPERAND = {
    "INCA",
    "INCB",
    "INCC",

    "INCXA",
    "INCXB",
    "DECXA",
    "DECXB",

    "DECC",

    "ADAAB",
    "ADABB",
    "SBAAB",
    "SBABB",
    "SBBAB",

    "SWPAB",
    "SWPADS",
    "SWPACS",
    "SWPAES",

    "SWPXAXB",
    "SWPXAAB",
    "SWPXBAB",

    "FJMPA",

    "PUSHA",
    "PUSHB",
    "POPA",
    "POPB",

    "DECA",
    "DECB",

    "LDAC",
    "LDBC",
    "SRAC",
    "SRBC",

    "IRET",

    # EZMEM
    "STRAB",

    # FUNCY
    "RET",

    "HLT",
}


JUMPS = {
    "JMP",
    "JAZ",
    "JBZ",
    "JCZ",
}


PREPROCESSED = {
    "DB",
    "DW",
}


def parse_value(s, labels=None):
    s = s.strip()

    if labels is not None and s in labels:
        return labels[s]

    if len(s) >= 3 and s[0] == "'" and s[-1] == "'":
        if len(s) != 3:
            raise ValueError(f"invalid character literal: {s}")

        return ord(s[1])

    try:
        return int(s, 0)
    except ValueError:
        raise ValueError(f"unknown value: {s}")


def instruction_size(mnemonic):
    if mnemonic == "OUT":
        return 5

    if mnemonic == "STRIMMIMM":
        return 5

    if mnemonic in NO_OPERAND:
        return 1

    if mnemonic in ONE_BYTE_OPERAND:
        return 2

    if mnemonic in TWO_BYTE_OPERAND:
        return 3

    if mnemonic in FOUR_BYTE_OPERAND:
        return 5

    if mnemonic in JUMPS:
        return 5

    raise ValueError(f"assembler doesn't know how to encode {mnemonic}")


def do_byteinsertion(mnemonic, args, labels):
    if mnemonic == "DB":
        out = []

        for arg in args:
            value = parse_value(arg, labels)

            if not 0 <= value <= 0xff:
                raise ValueError(f"byte out of range: {value}")

            out.append(value)

        return out

    if mnemonic == "DW":
        out = []

        for arg in args:
            value = parse_value(arg, labels)

            if not 0 <= value <= 0xffff:
                raise ValueError(f"word out of range: {value}")

            out.append(value & 0xff)
            out.append((value >> 8) & 0xff)

        return out

    return []


def assemble_instruction(line, labels):
    parts = line.replace(",", " ").split()

    if not parts:
        return []

    mnemonic = parts[0].upper()
    args = parts[1:]

    if mnemonic in PREPROCESSED:
        return do_byteinsertion(mnemonic, args, labels)

    if mnemonic not in OPCODES:
        raise ValueError(f"unknown instruction: {mnemonic}")

    opcode = OPCODES[mnemonic]

    if mnemonic in NO_OPERAND:
        if args:
            raise ValueError(f"{mnemonic} takes no operands")

        return [opcode]

    if mnemonic in ONE_BYTE_OPERAND:
        if len(args) != 1:
            raise ValueError(f"{mnemonic} takes one operand")

        value = parse_value(args[0], labels)

        if not 0 <= value <= 0xff:
            raise ValueError(f"operand out of range: {value}")

        return [
            opcode,
            value
        ]

    if mnemonic in TWO_BYTE_OPERAND:
        if len(args) != 1:
            raise ValueError(f"{mnemonic} takes one operand")

        value = parse_value(args[0], labels)

        if not 0 <= value <= 0xffff:
            raise ValueError(f"operand out of range: {value}")

        return [
            opcode,
            value & 0xff,
            (value >> 8) & 0xff
        ]

    if mnemonic in FOUR_BYTE_OPERAND:
        if len(args) != 1:
            raise ValueError(f"{mnemonic} takes one operand")

        value = parse_value(args[0], labels)

        if not 0 <= value <= 0xffffffff:
            raise ValueError(f"operand out of range: {value}")

        return [
            opcode,
            value & 0xff,
            (value >> 8) & 0xff,
            (value >> 16) & 0xff,
            (value >> 24) & 0xff
        ]

    if mnemonic == "STRIMMIMM":
        if len(args) != 2:
            raise ValueError("STRIMMIMM takes two operands")

        address = parse_value(args[0], labels)
        value = parse_value(args[1], labels)

        if not 0 <= address <= 0xffff:
            raise ValueError(f"address out of range: {address}")

        if not 0 <= value <= 0xffff:
            raise ValueError(f"value out of range: {value}")

        return [
            opcode,
            address & 0xff,
            (address >> 8) & 0xff,
            value & 0xff,
            (value >> 8) & 0xff
        ]

    if mnemonic == "OUT":
        if len(args) != 2:
            raise ValueError("OUT takes two operands")

        port = parse_value(args[0], labels)
        value = parse_value(args[1], labels)

        if not 0 <= port <= 0xff:
            raise ValueError(f"port out of range: {port}")

        if not 0 <= value <= 0xffff:
            raise ValueError(f"value out of range: {value}")

        return [
            opcode,
            port,
            0,
            value & 0xff,
            (value >> 8) & 0xff
        ]

    if mnemonic in JUMPS:
        if len(args) != 1:
            raise ValueError(f"{mnemonic} takes one operand")

        address = parse_value(args[0], labels)

        if not 0 <= address <= 0xffffffff:
            raise ValueError(f"address out of range: {address}")

        return [
            opcode,
            address & 0xff,
            (address >> 8) & 0xff,
            (address >> 16) & 0xff,
            (address >> 24) & 0xff
        ]

    raise ValueError(f"assembler doesn't know how to encode {mnemonic}")


def assemble(source):
    lines = source.splitlines()

    labels = {}
    offset = 0

    for line_number, original_line in enumerate(lines, 1):
        line = original_line.split(";", 1)[0].strip()

        if not line:
            continue

        while ":" in line:
            label, line = line.split(":", 1)

            label = label.strip()
            line = line.strip()

            if not label:
                raise ValueError(
                    f"line {line_number}: empty label"
                )

            if label in labels:
                raise ValueError(
                    f"line {line_number}: duplicate label '{label}'"
                )

            labels[label] = CODE_START + offset

            if not line:
                break

        if not line:
            continue

        parts = line.replace(",", " ").split()
        mnemonic = parts[0].upper()

        if mnemonic in PREPROCESSED:
            offset += len(do_byteinsertion(mnemonic, parts[1:], labels))
            continue

        if mnemonic not in OPCODES:
            raise ValueError(
                f"line {line_number}: unknown instruction: {mnemonic}"
            )

        try:
            offset += instruction_size(mnemonic)
        except ValueError as e:
            raise ValueError(f"line {line_number}: {e}")

    output = bytearray()

    for line_number, original_line in enumerate(lines, 1):
        try:
            line = original_line.split(";", 1)[0].strip()

            if not line:
                continue

            while ":" in line:
                _, line = line.split(":", 1)
                line = line.strip()

                if not line:
                    break

            if not line:
                continue

            output.extend(
                assemble_instruction(line, labels)
            )

        except ValueError as e:
            raise ValueError(
                f"line {line_number}: {e}"
            )

    return output


def main():
    if len(sys.argv) != 3:
        print(
            f"usage: {sys.argv[0]} input.asm output.bin"
        )
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    with open(input_file, "r") as f:
        source = f.read()

    try:
        binary = assemble(source)

    except ValueError as e:
        print(f"error: {e}")
        sys.exit(1)

    with open(output_file, "wb") as f:
        f.write(binary)

    print(f"assembled {len(binary)} bytes")


if __name__ == "__main__":
    main()
