#include "ue-elf.h"

static uint8_t read_buf[ELF_READ_BUF_SIZE];

int elf_parse_header(FILE* fp, Elf64_Ehdr* header) {
  uint8_t* ptr = read_buf;

  // Read the first 64-bytes, which is the size of a 64-bit ELF header
  if (fread(read_buf, ELF64_HEADER_SIZE, 1, fp) != 1) {
    return -ELF_ERR_FILE_TOO_SMALL;
  }

  // Check the magic: "\x7FELF"
  if (READ_U32(ptr) != 0x464c457f) {
    return -ELF_ERR_BAD_MAGIC;
  }
  ptr += 4;

  // Check that the ELF is 64-bit
  if (*ptr++ != ELFCLASS64) {
    return -ELF_ERR_NON_64BIT;
  }

  // Check endianness is LE
  if (*ptr++ != ELFDATA2LSB) {
    return -ELF_ERR_NON_LE;
  }

  // Check version is 1
  if (*ptr++ != 1) {
    return -ELF_ERR_BAD_VERSION;
  }

  // Check OS ABI is correct
  if (!((*ptr == ELFOSABI_NONE) || (*ptr == ELFOSABI_LINUX))) {
    return -ELF_ERR_BAD_ABI;
  }
  ptr++;

  // Skip over the ABI Version field
  ptr++;

  // Skip over reserved padding
  ptr += 7;

  // Check the file type is executable
  uint16_t elf_type = READ_U16(ptr);
  if (elf_type != ET_EXEC) {
    return -ELF_ERR_EXECUTABLE;
  }
  ptr += 2;

  // Check the ISA - should be "No specific instruction set"
  uint16_t elf_isa = READ_U16(ptr);
  if (elf_isa != EM_X86_64) {
    return -ELF_ERR_ISA;
  }
  ptr += 2;

  // Long ELF version number is 1
  if (READ_U32(ptr) != 1) {
    // return -ELF_ERR_BAD_VERSION;
  }
  ptr += 4;

  // At this point, we can say that the header is at least valid *enough*
  memcpy(header, read_buf, ELF64_HEADER_SIZE);

  return 0;
}

int elf_parse_program_headers(FILE* fp, const Elf64_Ehdr* header, Elf64_Phdr* phdrs) {
  Elf64_Phdr* phdr = phdrs;

  size_t num_headers = header->e_phnum;
  size_t program_offset = header->e_phoff;
  size_t header_size = header->e_phentsize;

  for (int i = 0; i < num_headers; i++) {
    // Locate the relevant header within the ELF
    if (fseek(fp, program_offset + (header_size * i), SEEK_SET) != 0) {
      return -ELF_ERR_BAD_PHDR;
    }

    // Read it into the buffer
    if (fread(phdr, header_size, 1, fp) != 1) {
      return -ELF_ERR_BAD_PHDR;
    }

    // Point to the next program header in the buffer
    phdr++;
  }

  return 0;
}

static char* elf_errors[] = {
  "Unknown",
  "The ELF file was too small to read a full header",
  "Bad magic number",
  "Only 64-Bit ELF files supported",
  "Only little-endian ELF files supported",
  "Bad version number",
  "Only System V OS ABI supported",
  "ELF file is not an executable",
  "Unsupported ISA",
  "Program headers couldn't be read",
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
