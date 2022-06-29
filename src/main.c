#include "main.h"
#include "ue-elf.h"
#include "ue-memory.h"
#include "cpu.h"

#define TEST_BIN "./testcases/true"

int main(int argc, char** argv) {
  FILE* fp = fopen(TEST_BIN, "rb");

  Elf64_Ehdr elf_header = {0};
  int ret = elf_parse_header(fp, &elf_header);

  if (ret != 0) {
    printf("ELF Parsing Error: %s\n", elf_err_message(ret));
    return 1;
  }

  Elf64_Phdr* elf_program_headers = malloc(elf_header.e_phnum * elf_header.e_phentsize);
  if (!elf_program_headers) {
    printf("Couldn't allocate memory for program headers\n");
    return 1;
  }

  ret = elf_parse_program_headers(fp, &elf_header, elf_program_headers);
  if (ret != 0) {
    printf("ELF Program Headers Error: %s\n", elf_err_message(ret));
    return 1;
  }

  for (size_t i = 0; i < elf_header.e_phnum; i++) {
    ret = load_memory_region(&elf_program_headers[i], fp);
    if (ret != 0) {
      printf("Memory region load error: %s\n", memory_err_message(ret));
      return 1;
    }
  }

  // The raw headers are no longer needed after the memory regions are loaded
  free(elf_program_headers);

  cpu_x86_64_t cpu = {
    .rip = elf_header.e_entry
  };

  x86_64_instr_t instr;
  while (1) {
    printf("[0x%016x]\n", cpu.rip);
    ret = fetch_decode_execute(&cpu);
    if (ret != 0) {
      printf("Execution error: %s\n", cpu_err_message(ret));
      return 1;
    }
  }

  free_memory_regions();
  fclose(fp);

  return 0;
}
