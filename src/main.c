#include "main.h"
#include "ue-elf.h"

#define TEST_BIN "/bin/true"
// #define TEST_BIN "/home/francis/repos/x86_64-userspace-emu/testcases/true"

int main(int argc, char** argv) {
  FILE* fp = fopen(TEST_BIN, "rb");

  Elf64_Ehdr elf_header = {0};
  int ret = elf_parse_header(fp, &elf_header);

  if (ret != 0) {
    printf("ELF Parsing Error: %s\n", elf_err_message(ret));
    return 1;
  }

  fclose(fp);

  return 0;
}
