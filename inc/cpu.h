#ifndef UE_CPU_H
#define UE_CPU_H

#include "common.h"

enum {
  ENDBR64,
  XOR_31,
  MOV_89
};

typedef struct rflags_t {
  uint64_t cf   : 1;
  uint64_t res0 : 1;
  uint64_t pf   : 1;
  uint64_t res1 : 1;
  uint64_t af   : 1;
  uint64_t res2 : 1;
  uint64_t zf   : 1;
  uint64_t sf   : 1;
  uint64_t tf   : 1;
  uint64_t if_  : 1;
  uint64_t df   : 1;
  uint64_t of   : 1;
  uint64_t iopl : 2;
  uint64_t nt   : 1;
  uint64_t res3 : 1;
  uint64_t rf   : 1;
  uint64_t vm   : 1;
  uint64_t ac   : 1;
  uint64_t vif  : 1;
  uint64_t vip  : 1;
  uint64_t id   : 1;
} rflags_t;

typedef struct cr0_t {
  uint64_t pe   : 1;
  uint64_t mp   : 1;
  uint64_t em   : 1;
  uint64_t ts   : 1;
  uint64_t et   : 1;
  uint64_t ne   : 1;
  uint64_t res0 : 10;
  uint64_t wp   : 1;
  uint64_t res1 : 1;
  uint64_t am   : 1;
  uint64_t res2 : 10;
  uint64_t nw   : 1;
  uint64_t cd   : 1;
  uint64_t pg   : 1;
} cr0_t;

// Note: When CR4.PCIDE == 1, the first 12 bits are actually PCID
typedef struct cr3_t {
  uint64_t res0  : 3;
  uint64_t pwt   : 1;
  uint64_t res1  : 1;
  uint64_t pcd   : 1;
  uint64_t res2  : 6;

  uint64_t paddr : 52;
} cr3_t;

typedef struct cr4_t {
  uint64_t vme        : 1;
  uint64_t pvi        : 1;
  uint64_t tsd        : 1;
  uint64_t de         : 1;
  uint64_t pse        : 1;
  uint64_t pae        : 1;
  uint64_t mce        : 1;
  uint64_t pge        : 1;
  uint64_t pce        : 1;
  uint64_t psfxsr     : 1;
  uint64_t osxmmexcpt : 1;
  uint64_t umip       : 1;
  uint64_t res0       : 1;
  uint64_t vmxe       : 1;
  uint64_t smxe       : 1;
  uint64_t res1       : 1;
  uint64_t fsgsbase   : 1;
  uint64_t pcide      : 1;
  uint64_t osxsave    : 1;
  uint64_t res2       : 1;
  uint64_t smep       : 1;
  uint64_t smap       : 1;
  uint64_t pke        : 1;
  uint64_t cet        : 1;
  uint64_t pks        : 1;
} cr4_t;

typedef struct cpu_x86_64_t {
  uint64_t rax;
  uint64_t rbx;
  uint64_t rcx;
  uint64_t rdx;
  uint64_t rsi;
  uint64_t rdi;
  uint64_t rsp;
  uint64_t rbp;
  uint64_t r8;
  uint64_t r9;
  uint64_t r10;
  uint64_t r11;
  uint64_t r12;
  uint64_t r13;
  uint64_t r14;
  uint64_t r15;

  uint64_t rip;

  uint16_t cs;
  uint16_t ds;
  uint16_t ss;
  uint16_t es;
  uint16_t fs;
  uint16_t gs;

  rflags_t rflags;

  cr0_t     cr0;
  uint64_t  cr2;
  cr4_t     cr4;
  uint64_t  cr8;
} cpu_x86_64_t;

typedef struct rex_prefix_t {
  uint8_t b            : 1;
  uint8_t x            : 1;
  uint8_t r            : 1;
  uint8_t w            : 1;
  uint8_t const_nibble : 4;
} rex_prefix_t;

typedef struct modrm_t {
  uint8_t rm  : 3;
  uint8_t reg : 3;
  uint8_t mod : 2;
} modrm_t;

enum {
  modrm_rax,  // 0b0000
  modrm_rcx,  // 0b0001
  modrm_rdx,  // 0b0010
  modrm_rbx,  // 0b0011
  modrm_rsp,  // 0b0100
  modrm_rbp,  // 0b0101
  modrm_rsi,  // 0b0110
  modrm_rdi,  // 0b0111
  modrm_r8,   // 0b1000
  modrm_r9,   // 0b1001
  modrm_r10,  // 0b1010
  modrm_r11,  // 0b1011
  modrm_r12,  // 0b1100
  modrm_r13,  // 0b1101
  modrm_r14,  // 0b1110
  modrm_r15,  // 0b1111
};

typedef struct prefixes_t {
  bool p66;
  bool pREX;
} prefixes_t;

#define MAX_INSTRUCTIONS_BYTES  (15)
typedef struct x86_64_instr_t {
  uint16_t type;
  uint8_t size;
  uint8_t as_bytes[15];

  modrm_t modrm;
  rex_prefix_t rex;
  prefixes_t prefixes;

  // registers
  // immediates
  // SIB etc

} x86_64_instr_t;

int fetch_decode_execute(cpu_x86_64_t* cpu);
int decode_at_address(const uint64_t address, cpu_x86_64_t* cpu, x86_64_instr_t* instr_out);
uint64_t* reg_from_nibble(const cpu_x86_64_t* cpu, const uint8_t nibble);

enum {
  CPU_ERR_UNKNOWN = 0,
  CPU_ERR_UNABLE_TO_DECODE,
  CPU_ERR_INVALID_MODRM_INDEX,
  CPU_ERR_UNABLE_TO_EXECUTE,
  CPU_ERR_UNABLE_TO_READ,
  // ...
  CPU_ERR_NUM_ERRORS
};
char* cpu_err_message(int errorIndex);

#endif // UE_CPU_H
