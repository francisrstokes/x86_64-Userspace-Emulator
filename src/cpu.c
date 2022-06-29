#include "cpu.h"
#include "ue-memory.h"

#define ENDBR64_U32   (0xfa1e0ff3)
#define XOR_31_OPCODE (0x31)

int decode_at_address(const uint64_t address, cpu_x86_64_t* cpu, x86_64_instr_t* instr_out) {
  uint32_t next_u32;
  if (!read_u32(cpu->rip, &next_u32)) {
    return -CPU_ERR_UNABLE_TO_READ;
  }

  // Special case these kinds of sequences for now
  if (next_u32 == ENDBR64_U32) {
    instr_out->type = ENDBR64;
    instr_out->size = 4;
    memcpy(instr_out->as_bytes, &next_u32, instr_out->size);
    return 0;
  }

  uint8_t next_u8;
  uint64_t offset = 0;

  // Search for prefixes
  while (true) {
    if (!read_u8(cpu->rip + offset, &next_u8)) {
      return -CPU_ERR_UNABLE_TO_READ;
    }

    if (next_u8 == 0x66) {
      instr_out->prefixes.p66 = true;
      offset += 1;
      continue;
    }

    if ((next_u8 >> 4) == 0b0100) {
      instr_out->prefixes.pREX = true;
      memcpy(&instr_out->rex, &next_u8, 1);
      offset += 1;
      continue;
    }

    break;
  }

  if (next_u8 == XOR_31_OPCODE) {
    instr_out->size = 2;
    instr_out->type = XOR_31;

    next_u8 = (next_u32 >> 8) & 0xff;
    memcpy(&instr_out->modrm, &next_u8, 1);

    instr_out->as_bytes[0] = 0x31;
    instr_out->as_bytes[1] = next_u8;

    return 0;
  }

  return -CPU_ERR_UNABLE_TO_DECODE;
}

int fetch_decode_execute(cpu_x86_64_t* cpu) {
  x86_64_instr_t instr = {0};

  int ret = decode_at_address(cpu->rip, cpu, &instr);
  if (ret != 0) {
    return ret;
  }

  switch (instr.type) {

    case ENDBR64: {
      cpu->rip += instr.size;
      return 0;
    }

    case XOR_31: {
      // By default, treat as a 32 bit operation
      uint64_t mask = 0xffffffff;

      if (instr.prefixes.p66) {
        mask = 0xffff;
      } else if (instr.prefixes.pREX) {
        mask = 0xffffffffffffffff;
      }

      uint64_t* src = reg_from_nibble(cpu, instr.modrm.reg);
      uint64_t* dst = reg_from_nibble(cpu, instr.modrm.rm);

      if (src == NULL || dst == NULL) {
        return -CPU_ERR_INVALID_MODRM_INDEX;
      }

      // Compute the xor
      *dst ^= *src;

      // Mask to appropriate size
      *dst &= mask;

      cpu->rip += instr.size;
      return 0;
    }
  }

  return 1;
}

uint64_t* reg_from_nibble(const cpu_x86_64_t* cpu, const uint8_t nibble) {
  switch (nibble) {
    case modrm_rax: return &cpu->rax;
    case modrm_rcx: return &cpu->rcx;
    case modrm_rdx: return &cpu->rdx;
    case modrm_rbx: return &cpu->rbx;
    case modrm_rsp: return &cpu->rsp;
    case modrm_rbp: return &cpu->rbp;
    case modrm_rsi: return &cpu->rsi;
    case modrm_rdi: return &cpu->rdi;
    case modrm_r8:  return &cpu->r8;
    case modrm_r9:  return &cpu->r9;
    case modrm_r10: return &cpu->r10;
    case modrm_r11: return &cpu->r11;
    case modrm_r12: return &cpu->r12;
    case modrm_r13: return &cpu->r13;
    case modrm_r14: return &cpu->r14;
    case modrm_r15: return &cpu->r15;
  }
  return NULL;
}

static char* elf_errors[] = {
  "Unknown",
  "Unable to decode instruction",
  "Invalid ModRM index",
  "Unable to execute instruction",
  "Unable to fetch instruction bytes from memory",
};

char* elf_err_message(int errorIndex) {
  if (errorIndex < 0) {
    errorIndex *= -1;
  }

  if (errorIndex >= ELF_ERR_NUM_ERRORS) {
    return elf_errors[ELF_ERR_UNKNOWN];
  }
  return elf_errors[errorIndex];
}