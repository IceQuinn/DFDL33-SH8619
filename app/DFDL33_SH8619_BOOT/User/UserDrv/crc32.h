#ifndef __CRC32_H__
#define __CRC32_H__

#include <stdint.h>

uint32_t crc32(void* ptr_buf, int nLength, uint32_t *data_crc);

#endif
