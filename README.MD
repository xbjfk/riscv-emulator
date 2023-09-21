# RISC V emulator
This is a RISC V emulator supporting RV32I.

## Info
Currently, only raw binaries are supported (no ELF files). You can use `objcopy` to create a raw binary: `llvm-objcopy -S -O binary X.elf X.bin`. Code is loaded to 0x1000 and UART is located at 0x200.

## Compilation
 - Generate opcode header file: `cd riscv-opcodes && ./parse.py rv64_i rv_i`
 - Compile: `cc main.c`

## TODO
 - [ ] Branch instruction / Jump
 - [ ] RV64I
 - [ ] S extensions
 - [ ] SBI / Device tree.
