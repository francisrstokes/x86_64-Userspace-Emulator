#ifndef UE_ELF_H
#define UE_ELF_H

#include "common.h"
#include "elf.h"

#define ELF_READ_BUF_SIZE (512)
#define ELF64_HEADER_SIZE (64)

enum {
  ELF_ERR_UNKNOWN = 0,
  ELF_ERR_FILE_TOO_SMALL,
  ELF_ERR_BAD_MAGIC,
  ELF_ERR_NON_64BIT,
  ELF_ERR_NON_LE,
  ELF_ERR_BAD_VERSION,
  ELF_ERR_BAD_ABI,
  ELF_ERR_EXECUTABLE,
  ELF_ERR_ISA,
  ELF_ERR_BAD_PHDR,
  // ...
  ELF_ERR_NUM_ERRORS
};
char* elf_err_message(int errorIndex);

int elf_parse_header(FILE* fp, Elf64_Ehdr* header);
int elf_parse_program_headers(FILE* fp, const Elf64_Ehdr* header, Elf64_Phdr* phdr);

#endif // UE_ELF_H
