#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdlib.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#define READ_U32(ptr) (*((uint32_t*)(ptr)))
#define READ_U16(ptr) (*((uint16_t*)(ptr)))

size_t GetFileSize(FILE* fp);

#endif // COMMON_H
