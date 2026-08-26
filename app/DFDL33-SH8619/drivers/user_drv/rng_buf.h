#ifndef _RNG_BUF_H_INCLUDE__
#define _RNG_BUF_H_INCLUDE__

// 这是一个特殊的双倍缓冲环形缓冲区, 实际内存字节数是可用字节数的两倍
// 只要在缓冲区内获取的读指针, 都可以读到连续的字节, 不用回头, 方便应用软件

#include <stdio.h>
#include <stdint.h>
#include "rtdef.h"

struct rngbuf_queue
{
    uint16_t buf_source;
    uint16_t len;
};


// 简单的环形缓冲区数据结构及管理接口
struct rng_buf
{
    // 根据实际需求,在外面指定缓冲区 (单片机无法动态申请内存)
    size_t      buf_total_size;
    int         read_idx;
    int         write_idx;
    int         full;   // 1=满了, 0=未满 
    int         mode;   // 缓冲区模式, 0=常规模式, 1=双倍模式
    unsigned char* buf; // 注意缓存缓冲区的存储单位为字节
};

#define RNG_BUF_MODE_SINGLE     0   // 单倍模式
#define RNG_BUF_MODE_DOUBLE     1   // 双倍模式

// 初始化, 将内存与管理结点关联
void RngBufInit(struct rng_buf* p_rng_buf, void* dat, size_t len, int mode);

// 写入指定字节到缓冲区, 返回实际写入数, -1表示缓冲区满, 无法写入
//int RngBufWrite(struct rng_buf* p_rng_buf, void* dat, size_t len) SECTION(".user_app");
int RngBufWrite(struct rng_buf* p_rng_buf, void* dat, size_t len);

// 从缓冲区读出指定长度的字节, 返回实际读到的字节数
int RngBufRead(struct rng_buf* p_rng_buf, void* dat, size_t len);

// 获取当前读指针, 计算FFT时节省内存开销
unsigned char* RngBufReadPtrGet(struct rng_buf* p_rng_buf);

// 移动环形缓冲区的读指针，在当前读指针的基础上加上偏移offset
void RngBufReadPtrAhead(struct rng_buf* p_rng_buf, size_t offset);

int RngBufIsFull(struct rng_buf* p_rng_buf);

//size_t RngBufDataSize(struct rng_buf* p_rng_buf) SECTION(".user_app");
size_t RngBufDataSize(struct rng_buf* p_rng_buf);

//size_t RngBufFreeSize(struct rng_buf* p_rng_buf) SECTION(".user_app");
size_t RngBufFreeSize(struct rng_buf* p_rng_buf);

//void RngBufClear(struct rng_buf* p_rng_buf) SECTION(".user_app");
void RngBufClear(struct rng_buf* p_rng_buf);

#endif
