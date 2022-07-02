#include "ue-memory.h"

static memory_region_t* region_ll = NULL;
static size_t num_regions = 0;

memory_region_t* get_memory_regions(void) {
  return region_ll;
}

size_t get_num_memory_regions(void) {
  return num_regions;
}

int free_memory_regions(void) {
  memory_region_t* temp;
  while (region_ll) {
    // Free the actual memory
    free(region_ll->buffer);

    // Keep track of the region so we can move the pointer to the next region
    temp = region_ll;
    region_ll = region_ll->next;

    // Free the region data itself
    free(temp);
  }
  return 0;
}

static memory_region_t* get_last_region(void) {
  if (region_ll == NULL) return NULL;

  memory_region_t* ptr = region_ll;
  while (ptr->next != NULL) {
    ptr = ptr->next;
  }
  return ptr;
}

int load_memory_region(Elf64_Phdr* program_header, FILE* fp) {
  // Allocate memory for a memory_region_t to hold region data
  memory_region_t* region = calloc(1, sizeof(memory_region_t));
  if (!region) {
    return -MEM_ERR_MALLOC;
  }

  // If this is the first region, just set it in the list
  if (region_ll == NULL) {
    region_ll = region;
  } else {
    // Otherwise, append to the end of the linked list
    get_last_region()->next = region;
  }

  // We're only going to allocate memory in this region if the segment specifies it
  if (program_header->p_memsz > 0) {
  // Allocate a buffer with enough space for the bytes (according to p_memsz)
    region->buffer = calloc(1, program_header->p_memsz);
    if (!region->buffer) {
      return -MEM_ERR_MALLOC;
    }

    // Seek to the offset for this segments data
    if (fseek(fp, program_header->p_offset, SEEK_SET) != 0) {
      return -MEM_ERR_ELF_READ;
    }

    // Read p_filesz bytes from the file into the buffer
    if (fread(region->buffer, program_header->p_filesz, 1, fp) != 1) {
      return -MEM_ERR_ELF_READ;
    }
  }

  // Copy the header data to the region struct
  memcpy(&region->header, program_header, sizeof(Elf64_Phdr));

  // Success
  return 0;
}

bool region_contains_address(memory_region_t* region, uint64_t address) {
  return (
    (region)
    && (address >= region->header.p_vaddr)
    && (address < (region->header.p_vaddr + region->header.p_memsz))
  );
}

static memory_region_t* find_region(uint64_t address) {
  memory_region_t* region = get_memory_regions();
  while (region) {
    if (region_contains_address(region, address)) break;
    region = region->next;
  }
  return region;
}

bool read_u8(uint64_t address, uint8_t* data_out) {
  memory_region_t* region = find_region(address);
    if (!region) return false;

  uint64_t region_offset = address - region->header.p_vaddr;
  *data_out = region->buffer[region_offset];
  return true;
}

bool read_u16(uint64_t address, uint16_t* data_out) {
  memory_region_t* region = find_region(address);
  if (!region) return false;

  // Ensure that all the bytes are in this region
  if (!region_contains_address(region, address + 1)) {
    // TODO: Scan other regions to see if this crosses a segment boundary
    return false;
  }

  uint64_t region_offset = address - region->header.p_vaddr;
  *data_out =  *((uint16_t*)(region->buffer + region_offset));
  return true;
}

bool read_u32(uint64_t address, uint32_t* data_out) {
  memory_region_t* region = find_region(address);
  if (!region) return false;

  // Ensure that all the bytes are in this region
  if (!region_contains_address(region, address + 3)) {
    // TODO: Scan other regions to see if this crosses a segment boundary
    return false;
  }

  uint64_t region_offset = address - region->header.p_vaddr;
  *data_out =  *((uint32_t*)(region->buffer + region_offset));
  return true;
}

bool read_u64(uint64_t address, uint64_t* data_out) {
  memory_region_t* region = find_region(address);
  if (!region) return false;

  // Ensure that all the bytes are in this region
  if (!region_contains_address(region, address + 7)) {
    // TODO: Scan other regions to see if this crosses a segment boundary
    return false;
  }

  uint64_t region_offset = address - region->header.p_vaddr;
  *data_out =  *((uint64_t*)(region->buffer + region_offset));
  return true;
}

static char* mem_errors[] = {
  "Unknown",
  "Unable to allocate memory for memory region",
  "Unable to read ELF data into memory region",
};

char* memory_err_message(int errorIndex) {
  if (errorIndex < 0) {
    errorIndex *= -1;
  }

  if (errorIndex >= ELF_ERR_NUM_ERRORS) {
    return mem_errors[ELF_ERR_UNKNOWN];
  }
  return mem_errors[errorIndex];
}