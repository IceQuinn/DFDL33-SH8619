#ifndef __MY_VSNPRINTF_H__
#define __MY_VSNPRINTF_H__

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define ALIGN_LEFT  (1 << 0)
#define ALIGN_ZERO  (1 << 1)
#define SIGN_PLUS   (1 << 2)
#define SIGN_SPACE  (1 << 3)
#define PREFIX_HEX  (1 << 4)
#define UPPERCASE   (1 << 5)
#define SIGNED      (1 << 6)

typedef struct{
    char* buf;       // 输出缓冲区指针
    size_t size;     // 缓冲区总大小
    size_t count;    // 当前已写入字符数
}BufferState;

int my_vsnprintf(char* buf, size_t size, const char* fmt, va_list args);

#endif
