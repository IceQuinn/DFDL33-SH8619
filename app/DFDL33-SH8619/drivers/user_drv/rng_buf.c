#include "rng_buf.h"

void RngBufInit(struct rng_buf* p_rng_buf, void* dat, size_t len, int mode)
{
    p_rng_buf->buf = dat;
    //add by walt 20180130
    p_rng_buf->mode = mode;
    if(mode == 1)
        p_rng_buf->buf_total_size = len/2;
    else
        p_rng_buf->buf_total_size = len;
    
    RngBufClear(p_rng_buf);
}

/*
  1、AD采样用，AD采样数据写入缓存中
  2、RS485接收到数据写入到缓存中
 */
int RngBufWrite(struct rng_buf* p_rng_buf, void* dat, size_t len)
{
    size_t i = 0;
    size_t free_size = RngBufFreeSize(p_rng_buf);
    
    if(free_size == 0)
    {
        return -1;
    }
    
    if(len == 0)
    {
        return 0;
    }
    
    if(free_size < len)
    {
        len = free_size;
    }
    
    for(i=0; i<len; i++)
    {
        p_rng_buf->buf[p_rng_buf->write_idx] = ((unsigned char*)dat)[i];
        if(p_rng_buf->mode == RNG_BUF_MODE_DOUBLE)
        {
            // 双倍模式, 写入第二缓冲区
            p_rng_buf->buf[p_rng_buf->buf_total_size + p_rng_buf->write_idx] = ((unsigned char*)dat)[i];
        }
        p_rng_buf->write_idx++;
        p_rng_buf->write_idx %= p_rng_buf->buf_total_size;
        // 写满了
        if(p_rng_buf->write_idx == p_rng_buf->read_idx)
        {
            p_rng_buf->full = 1;
            break;
        }
    }
    
    return len;
}

//从缓存中读取数据――for串口RS485用
int RngBufRead(struct rng_buf* p_rng_buf, void* dat, size_t len)
{
    size_t i = 0;
    size_t data_size = RngBufDataSize(p_rng_buf);
    if(data_size < len)
        len = data_size;
    
    for(i=0; i<len; i++)
    {
        ((unsigned char*)dat)[i] = p_rng_buf->buf[p_rng_buf->read_idx];
        p_rng_buf->read_idx++;
        p_rng_buf->read_idx %= p_rng_buf->buf_total_size;
        p_rng_buf->full = 0;
    }
    
    return len;
}

unsigned char* RngBufReadPtrGet(struct rng_buf* p_rng_buf)
{
    return p_rng_buf->buf + p_rng_buf->read_idx;
}

void RngBufReadPtrAhead(struct rng_buf* p_rng_buf, size_t offset)
{
    size_t data_size = RngBufDataSize(p_rng_buf);//RngBufFreeSize(p_rng_buf);
    //if(p_rng_buf->full == 0 && free_size < offset)
    if(data_size < offset)
    {
        offset = data_size;
    }
    p_rng_buf->read_idx = (p_rng_buf->read_idx + offset)%p_rng_buf->buf_total_size;

    if(offset > 0)
       p_rng_buf->full = 0;
}

int RngBufIsFull(struct rng_buf* p_rng_buf)
{
    return p_rng_buf->full;
}

//Buf中还剩下多少字节空间
size_t RngBufFreeSize(struct rng_buf* p_rng_buf)
{
    return p_rng_buf->buf_total_size - RngBufDataSize(p_rng_buf);
}

//Buf中写入了多少个字节
size_t RngBufDataSize(struct rng_buf* p_rng_buf)
{
    if(p_rng_buf->full)
        return p_rng_buf->buf_total_size;
    else
        return (p_rng_buf->write_idx + p_rng_buf->buf_total_size - p_rng_buf->read_idx)%p_rng_buf->buf_total_size;
}

void RngBufClear(struct rng_buf* p_rng_buf)
{
    p_rng_buf->write_idx = 0;
    p_rng_buf->read_idx = 0;
    p_rng_buf->full = 0;
}
