#ifndef _CRC32_H_INCLUDE__
#define _CRC32_H_INCLUDE__

#include "drv_common.h"


uint32_t crc32(void* buf, uint32_t len);
uint32_t crc32_part(void* ptr_buf, int nLength, uint32_t *data_crc);

void sf_reset_crc(void);

uint32_t sf_crc_accumulate(void* buf, uint32_t nLength);

#endif


