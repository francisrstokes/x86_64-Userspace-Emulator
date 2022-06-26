#ifndef UE_MEMORY_H
#define UE_MEMORY_H

#include "common.h"
#include "ue-elf.h"

struct memory_region_t {
  uint8_t* buffer;
  Elf64_Phdr header;
  struct memory_region_t* next;
};

typedef struct memory_region_t memory_region_t;

memory_region_t* get_memory_regions(void);
size_t get_num_memory_regions(void);
int free_memory_regions(void);
int load_memory_region(Elf64_Phdr* program_header, FILE* fp);

enum {
  MEM_ERR_UNKNOWN = 0,
  MEM_ERR_MALLOC,
  MEM_ERR_ELF_READ,
  // ...
  MEM_ERR_NUM_ERRORS
};
char* memory_err_message(int errorIndex);

#endif //UE_MEMORY_H
