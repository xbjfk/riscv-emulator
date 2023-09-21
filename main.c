#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "riscv-opcodes/encoding.out.h"

#define MEMORY_SIZE 0x10000000
#define UART_ADDR 0x200
#define START_ADDRESS 0x1000

struct __attribute__((__packed__)) insn_r {
  unsigned opcode : 7;
  unsigned rd : 5;
  unsigned funct3 : 3;
  unsigned rs1 : 5;
  unsigned rs2 : 5;
  unsigned funct7 : 7;
};

struct __attribute__((__packed__)) insn_i {
  unsigned opcode : 7;
  unsigned rd : 5;
  unsigned funct3 : 3;
  unsigned rs1 : 5;
  int imm : 12;
};

struct __attribute__((__packed__)) insn_s {
  unsigned opcode : 7;
  unsigned imm_low : 5;
  unsigned funct3 : 3;
  unsigned rs1 : 5;
  unsigned rs2 : 5;
  int imm_high : 7;
};

struct __attribute__((__packed__)) insn_u {
  unsigned opcode : 7;
  unsigned rd : 5;
  int imm : 20;
};

static_assert(sizeof(struct insn_r) == 4, "struct insn_r is incorrect size");
static_assert(sizeof(struct insn_i) == 4, "struct insn_i is incorrect size");
static_assert(sizeof(struct insn_s) == 4, "struct insn_i is incorrect size");
static_assert(sizeof(struct insn_u) == 4, "struct insn_i is incorrect size");

// Ensure distinction between logical and arithmetic shift
static_assert((-5 >> 1) == -3, "arithmetic right shift is not correct");
static_assert(((unsigned)-5 >> 1) == 0x7FFFFFFD,
              "logical right shift is not correct");

static_assert((-1 & 3) == 3, "compiler does not use two's complement");

int insn_s_get_imm(struct insn_s insn) {
  return (insn.imm_high << 5) + insn.imm_low;
}

// Linux needs S abd Zicsr
int main(int argc, char **argv) {
  uint32_t pc = START_ADDRESS;
  uint32_t __attribute__((aligned(4))) x[32] = {0};
  char *memory = malloc(sizeof(char) * MEMORY_SIZE);
  assert(memory);
  size_t ret;
  FILE *code_file;

  if (argc != 2) {
    fputs("Incorrect number of arguments.\n", stderr);
    return EXIT_FAILURE;
  }

  code_file = fopen(argv[1], "rb");
  if (!code_file) {
    perror("fopen");
    return EXIT_FAILURE;
  }
  ret = fread(memory + START_ADDRESS, sizeof(*memory),
              MEMORY_SIZE - START_ADDRESS, code_file);
  if (!ret) {
    perror("fread");
    return EXIT_FAILURE;
  }

  uint32_t current_instruction;
  uint32_t store_offset;
  for (current_instruction = *(uint32_t *)(memory + pc); 1;
       current_instruction = *(uint32_t *)(memory + pc)) {
    // printf("PC: %lx, INSN: %x\n", pc, current_instruction);
    store_offset = 0;
    // I type
    if ((current_instruction & MASK_ADDI) == MATCH_ADDI) {
      struct insn_i insn = *(struct insn_i *)&current_instruction;
      if (insn.rd)
        x[insn.rd] = x[insn.rs1] + insn.imm;
    } else if ((current_instruction & MASK_SLTI) == MATCH_SLTI) {
      struct insn_i insn = *(struct insn_i *)&current_instruction;
      if (insn.rd)
        x[insn.rd] = (int32_t)x[insn.rs1] < insn.imm;
    } else if ((current_instruction & MASK_SLTIU) == MATCH_SLTIU) {
      struct insn_i insn = *(struct insn_i *)&current_instruction;
      if (insn.rd)
        x[insn.rd] = x[insn.rs1] < (uint32_t)insn.imm;
    } else if ((current_instruction & MASK_ANDI) == MATCH_ANDI) {
      struct insn_i insn = *(struct insn_i *)&current_instruction;
      if (insn.rd)
        x[insn.rd] = x[insn.rs1] & insn.imm;
    } else if ((current_instruction & MASK_ORI) == MATCH_ORI) {
      struct insn_i insn = *(struct insn_i *)&current_instruction;
      if (insn.rd)
        x[insn.rd] = x[insn.rs1] | insn.imm;
    } else if ((current_instruction & MASK_XORI) == MATCH_XORI) {
      struct insn_i insn = *(struct insn_i *)&current_instruction;
      if (insn.rd)
        x[insn.rd] = x[insn.rs1] ^ insn.imm;
    }
    // TODO: "the sign bit for all immediates is always held in bit 31 of the
    // instruction"
    else if ((current_instruction & MASK_SLLI) == MATCH_SLLI) {
      struct insn_i insn = *(struct insn_i *)&current_instruction;
      if (insn.rd)
        x[insn.rd] = x[insn.rs1]
                     << ((unsigned)insn.imm & 0b11111); // 0b111111 if rv64
    } else if ((current_instruction & MASK_SRLI) == MATCH_SRLI) {
      struct insn_i insn = *(struct insn_i *)&current_instruction;
      if (insn.rd)
        x[insn.rd] = x[insn.rs1] >> ((unsigned)insn.imm & 0b11111);
    } else if ((current_instruction & MASK_SRAI) == MATCH_SRAI) {
      struct insn_i insn = *(struct insn_i *)&current_instruction;
      // TODO: this is probably too many casts but better safe than sorry
      if (insn.rd)
        x[insn.rd] =
            (int32_t)x[insn.rs1] >> (int)((unsigned)insn.imm & 0b11111);
    }
    // U type (should U-immediate get be a function?)
    else if ((current_instruction & MASK_LUI) == MATCH_LUI) {
      struct insn_u insn = *(struct insn_u *)&current_instruction;
      if (insn.rd)
        x[insn.rd] = insn.imm << 12;
    } else if ((current_instruction & MASK_LUI) == MATCH_LUI) {
      struct insn_u insn = *(struct insn_u *)&current_instruction;
      if (insn.rd)
        x[insn.rd] = pc + (insn.imm << 12);
    }
    // R type - reg to reg
    else if ((current_instruction & MASK_ADD) == MATCH_ADD) {
      struct insn_r insn = *(struct insn_r *)&current_instruction;
      x[insn.rd] = x[insn.rs1] + x[insn.rs2];
    } else if ((current_instruction & MASK_SLT) == MATCH_SLT) {
      struct insn_r insn = *(struct insn_r *)&current_instruction;
      x[insn.rd] = (int32_t)x[insn.rs1] < (int32_t)x[insn.rs2];
    } else if ((current_instruction & MASK_SLTU) == MATCH_SLTU) {
      struct insn_r insn = *(struct insn_r *)&current_instruction;
      x[insn.rd] = x[insn.rs1] < x[insn.rs2];
    } else if ((current_instruction & MASK_AND) == MATCH_AND) {
      struct insn_r insn = *(struct insn_r *)&current_instruction;
      x[insn.rd] = x[insn.rs1] & x[insn.rs2];
    } else if ((current_instruction & MASK_OR) == MATCH_OR) {
      struct insn_r insn = *(struct insn_r *)&current_instruction;
      x[insn.rd] = x[insn.rs1] | x[insn.rs2];
    } else if ((current_instruction & MASK_XOR) == MATCH_XOR) {
      struct insn_r insn = *(struct insn_r *)&current_instruction;
      x[insn.rd] = x[insn.rs1] ^ x[insn.rs2];
    } else if ((current_instruction & MASK_SLL) == MATCH_SLL) {
      struct insn_r insn = *(struct insn_r *)&current_instruction;
      x[insn.rd] = x[insn.rs1] << (x[insn.rs2] & 0b11111);
    } else if ((current_instruction & MASK_SRL) == MATCH_SRL) {
      struct insn_r insn = *(struct insn_r *)&current_instruction;
      x[insn.rd] = x[insn.rs1] << (x[insn.rs2] & 0b11111);
    } else if ((current_instruction & MASK_SRA) == MATCH_SRA) {
      struct insn_r insn = *(struct insn_r *)&current_instruction;
      x[insn.rd] = (int32_t)x[insn.rs1] << (int32_t)(x[insn.rs2] & 0b11111);
    } else if ((current_instruction & MASK_SUB) == MATCH_SUB) {
      struct insn_r insn = *(struct insn_r *)&current_instruction;
      x[insn.rd] = x[insn.rs1] - x[insn.rs2];
    }
    // Jump goes here
    // Load
    else if ((current_instruction & MASK_LW) == MATCH_LW) {
      struct insn_i insn = *(struct insn_i *)&current_instruction;
      x[insn.rd] = *(int32_t *)(memory + x[insn.rs1] + insn.imm);
    } else if ((current_instruction & MASK_LH) == MATCH_LH) {
      struct insn_i insn = *(struct insn_i *)&current_instruction;
      x[insn.rd] = *(int16_t *)(memory + x[insn.rs1] + insn.imm);
    } else if ((current_instruction & MASK_LHU) == MATCH_LHU) {
      struct insn_i insn = *(struct insn_i *)&current_instruction;
      x[insn.rd] = *(uint16_t *)(memory + x[insn.rs1] + insn.imm);
    } else if ((current_instruction & MASK_LB) == MATCH_LB) {
      struct insn_i insn = *(struct insn_i *)&current_instruction;
      x[insn.rd] = *(int8_t *)(memory + x[insn.rs1] + insn.imm);
    } else if ((current_instruction & MASK_LBU) == MATCH_LBU) {
      struct insn_i insn = *(struct insn_i *)&current_instruction;
      x[insn.rd] = *(uint8_t *)(memory + x[insn.rs1] + insn.imm);
    }
    // Store
    else if ((current_instruction & MASK_SW) == MATCH_SW) {
      struct insn_s insn = *(struct insn_s *)&current_instruction;
      store_offset = x[insn.rs1] + insn_s_get_imm(insn);
      memcpy(memory + store_offset, &x[insn.rs2], 4);
    } else if ((current_instruction & MASK_SH) == MATCH_SH) {
      struct insn_s insn = *(struct insn_s *)&current_instruction;
      store_offset = x[insn.rs1] + insn_s_get_imm(insn);
      memcpy(memory + store_offset, &x[insn.rs2], 2);
    } else if ((current_instruction & MASK_SB) == MATCH_SB) {
      struct insn_s insn = *(struct insn_s *)&current_instruction;
      store_offset = x[insn.rs1] + insn_s_get_imm(insn);
      memcpy(memory + store_offset, &x[insn.rs2], 1);
    } else {
      fputs("Illegal instruction!\n", stderr);
      return EXIT_FAILURE;
    }

    if (store_offset == UART_ADDR) {
      putchar(memory[UART_ADDR]);
    }
    x[0] = 0;
    if (pc == 0x1048)
      break;
    pc += 4;
  }
}
