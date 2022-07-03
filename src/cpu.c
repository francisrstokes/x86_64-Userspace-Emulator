#include "cpu.h"
#include "ue-memory.h"

#define ENDBR64_U32         (0xfa1e0ff3)
#define XOR_31_OPCODE       (0x31)
#define MOV_89_OPCODE       (0x89)
#define POP_58_MASK         (0x58)
#define SEXTEND_OP_OPCODE   (0x83)

#define SIGN_EXTEND_64      (0xffffffffffffff00)

static uint8_t parity(uint64_t x) {
  uint8_t count = 0;
  while (x) {
    count += x & 1;
    x >>= 1;
  }
  return (count & 1) ? 0 : 1;
}

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
      instr_out->as_bytes[offset] = next_u8;
      offset += 1;
      continue;
    }

    if ((next_u8 >> 4) == 0b0100) {
      instr_out->prefixes.pREX = true;
      instr_out->as_bytes[offset] = next_u8;
      memcpy(&instr_out->rex, &next_u8, 1);
      offset += 1;
      continue;
    }

    break;
  }

  if (next_u8 == XOR_31_OPCODE) {
    instr_out->size = 2 + offset;
    instr_out->type = XOR_31;

    if (!read_u8(cpu->rip + offset + 1, &next_u8)) {
      return -CPU_ERR_UNABLE_TO_READ;
    }
    memcpy(&instr_out->modrm, &next_u8, 1);

    instr_out->as_bytes[offset + 0] = XOR_31_OPCODE;
    instr_out->as_bytes[offset + 1] = next_u8;

    return 0;
  }

  // This opcode relates to multiple instructions,
  // depending on the "reg" field in the ModRM byte
  if (next_u8 == SEXTEND_OP_OPCODE) {
    // Read the ModRM byte
    if (!read_u8(cpu->rip + offset + 1, &next_u8)) {
      return -CPU_ERR_UNABLE_TO_READ;
    }
    memcpy(&instr_out->modrm, &next_u8, 1);

    if (instr_out->modrm.reg == SIGN_EXTEND_AND) {
      instr_out->type = AND_83;
    } else {
      return -CPU_ERR_NOT_IMPLEMENTED_YET;
    }

    instr_out->size = 3 + offset;
    instr_out->reg_index = instr_out->modrm.rm;

    // Read the imm8 to be sign extended
    if (!read_u8(cpu->rip + offset + 2, (uint8_t*)(&instr_out->imm64))) {
      return -CPU_ERR_UNABLE_TO_READ;
    }

    // Perform sign extension
    instr_out->imm64 |= (instr_out->imm64 & 0x80) ? SIGN_EXTEND_64 : 0;

    instr_out->as_bytes[offset + 0] = SEXTEND_OP_OPCODE;
    instr_out->as_bytes[offset + 1] = next_u8;
    instr_out->as_bytes[offset + 2] = instr_out->imm64 & 0xff;

    return 0;
  }

  if (instr_out->prefixes.pREX && instr_out->rex.w == 1 && next_u8 == MOV_89_OPCODE) {
    instr_out->size = 2 + offset;
    instr_out->type = MOV_89;

    if (!read_u8(cpu->rip + offset + 1, &next_u8)) {
      return -CPU_ERR_UNABLE_TO_READ;
    }
    memcpy(&instr_out->modrm, &next_u8, 1);

    instr_out->as_bytes[offset + 0] = 0x89;
    instr_out->as_bytes[offset + 1] = next_u8;

    return 0;
  }

  if ((next_u8 & POP_58_MASK) == POP_58_MASK) {
    instr_out->size = 1 + offset;
    instr_out->type = POP_58;
    instr_out->as_bytes[offset + 0] = next_u8;
    instr_out->reg_index = next_u8 & ~(POP_58_MASK);
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

      uint8_t r_bit = (instr.rex.r) << 3;
      uint8_t b_bit = (instr.rex.b) << 3;
      uint64_t* src = reg_from_nibble(cpu, r_bit | instr.modrm.reg);
      uint64_t* dst = reg_from_nibble(cpu, b_bit | instr.modrm.rm);

      if (src == NULL || dst == NULL) {
        return -CPU_ERR_INVALID_MODRM_INDEX;
      }

      // Clear any bits in the masked region
      *dst &= ~mask;

      // Compute the xor
      *dst |= (*src ^ *dst) & mask;

      // Set the flags
      cpu->rflags.cf = 0;
      cpu->rflags.of = 0;
      cpu->rflags.sf = (*dst & (1UL << 63UL)) > 0 ? 1 : 0;
      cpu->rflags.zf = (*dst == 0) ? 1 : 0;
      cpu->rflags.pf = parity(*dst);

      // Increment the instruction pointer
      cpu->rip += instr.size;
      return 0;
    }

    case AND_83: {
      // By default, treat as a 32 bit operation
      uint64_t mask = 0xffffffff;
      if (instr.prefixes.p66) {
        mask = 0xffff;
      } else if (instr.prefixes.pREX) {
        mask = 0xffffffffffffffff;
      }

      uint8_t w_bit = (instr.rex.w) << 3;
      uint64_t* dst = reg_from_nibble(cpu, w_bit | instr.reg_index);

      if (dst == NULL) {
        return -CPU_ERR_INVALID_MODRM_INDEX;
      }

      // Compute the and
      *dst = (*dst & ~mask) | (*dst & instr.imm64 & mask);

      // Set the flags
      cpu->rflags.cf = 0;
      cpu->rflags.of = 0;
      cpu->rflags.sf = (*dst & (1UL << 63UL)) > 0 ? 1 : 0;
      cpu->rflags.zf = (*dst == 0) ? 1 : 0;
      cpu->rflags.pf = parity(*dst);

      // Increment the instruction pointer
      cpu->rip += instr.size;
      return 0;
    }


    case MOV_89: {
      // By default, treat as a 32 bit operation
      uint64_t mask = 0xffffffff;
      if (instr.prefixes.p66) {
        mask = 0xffff;
      } else if (instr.prefixes.pREX) {
        mask = 0xffffffffffffffff;
      }

      uint8_t r_bit = (instr.rex.r) << 3;
      uint8_t b_bit = (instr.rex.b) << 3;
      uint64_t* src = reg_from_nibble(cpu, r_bit | instr.modrm.reg);
      uint64_t* dst = reg_from_nibble(cpu, b_bit | instr.modrm.rm);

      if (src == NULL || dst == NULL) {
        return -CPU_ERR_INVALID_MODRM_INDEX;
      }

      // Clear any bits in the masked region
      *dst &= ~mask;

      // Write the masked src to dst
      *dst |= *src & mask;

      // No flags affected with mov

      // Increment the instruction pointer
      cpu->rip += instr.size;
      return 0;
    }

    case POP_58: {
      // Determine destination register
      uint8_t b_bit = (instr.rex.b) << 3;
      uint64_t* dst = reg_from_nibble(cpu, b_bit | instr.reg_index);

      if (dst == NULL) {
        return -CPU_ERR_INVALID_MODRM_INDEX;
      }

      // Pop the actual value from the stack
      uint64_t stack_value;
      ret = pop_stack(cpu, &stack_value);
      if (ret != 0) {
        return ret;
      }

      // Write to the destination register
      *dst = stack_value;

      // No flags affected with pop

      // Increment the instruction pointer
      cpu->rip += instr.size;

      return 0;
    }
  }

  return 1;
}

int pop_stack(cpu_x86_64_t* cpu, uint64_t* data_out) {
  if (!read_u64(cpu->rsp, data_out)) {
    return -CPU_ERR_INVALID_STACK_POINTER;
  }
  cpu->rsp += 8;
  return 0;
}

int push_stack(cpu_x86_64_t* cpu, uint64_t data) {
  cpu->rsp -= 8;
  if (!write_u64(cpu->rsp, data)) {
    return -CPU_ERR_INVALID_STACK_POINTER;
  }
  return 0;
}

uint64_t* reg_from_nibble(const cpu_x86_64_t* cpu, const uint8_t nibble) {
  switch (nibble) {
    case modrm_rax: return (uint64_t*)&cpu->rax;
    case modrm_rcx: return (uint64_t*)&cpu->rcx;
    case modrm_rdx: return (uint64_t*)&cpu->rdx;
    case modrm_rbx: return (uint64_t*)&cpu->rbx;
    case modrm_rsp: return (uint64_t*)&cpu->rsp;
    case modrm_rbp: return (uint64_t*)&cpu->rbp;
    case modrm_rsi: return (uint64_t*)&cpu->rsi;
    case modrm_rdi: return (uint64_t*)&cpu->rdi;
    case modrm_r8:  return (uint64_t*)&cpu->r8;
    case modrm_r9:  return (uint64_t*)&cpu->r9;
    case modrm_r10: return (uint64_t*)&cpu->r10;
    case modrm_r11: return (uint64_t*)&cpu->r11;
    case modrm_r12: return (uint64_t*)&cpu->r12;
    case modrm_r13: return (uint64_t*)&cpu->r13;
    case modrm_r14: return (uint64_t*)&cpu->r14;
    case modrm_r15: return (uint64_t*)&cpu->r15;
  }
  return NULL;
}

static char* cpu_errors[] = {
  "Unknown",
  "Unable to decode instruction",
  "Invalid ModRM index",
  "Unable to execute instruction",
  "Unable to fetch instruction bytes from memory",
  "Unable to fetch from invalid stack pointer address",
  "Not yet implemented",
};

char* cpu_err_message(int errorIndex) {
  if (errorIndex < 0) {
    errorIndex *= -1;
  }

  if (errorIndex >= ELF_ERR_NUM_ERRORS) {
    return cpu_errors[ELF_ERR_UNKNOWN];
  }
  return cpu_errors[errorIndex];
}