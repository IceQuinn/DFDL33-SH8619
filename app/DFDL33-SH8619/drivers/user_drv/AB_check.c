/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-01-12     IP155       the first version
 */
#include "drv_common.h"

#include <stdbool.h>

#include "AB_check.h"

//移植修改此处
#define rt_kprintf  rt_kprintf


#define head_len    (sizeof(rcd_head))

extern uint16_t crc16(void* buf, uint32_t len);


/* 通用AB区校验接口
 * return 0校验正确，1校验错误
 *  */
int32_t AB_check(   int32_t (*read)(uint32_t, void *, uint32_t),    //数据读接口
                int32_t (*write)(uint32_t, void *, uint32_t),   //数据写接口
                uint32_t a_addr,                            //A区地址
                uint32_t b_addr,                            //B区地址
                void *buf,                                  //内存中的数据地址(指针),从摘要开始
                uint32_t len,                               //数据的长度(包括摘要)
                char *desc)                                 //描述
{
    bool bZoneAOK = false;
    bool bZoneBOK = false;
    rcd_head *p_head = buf;
/* --------------------------------------------------------------------------------------- A区校验 */
    read(a_addr, buf, head_len);
    if(p_head->len > (len - head_len))
    {
        rt_kprintf("%s len = %d, RAM Len = %d\n", desc, p_head->len, len);
        p_head->len = len - head_len;
    }
    read(a_addr, buf, head_len+p_head->len);

    uint16_t crc16_A = crc16((uint8_t *)buf + head_len, p_head->len);
    if(crc16_A == p_head->crc16b)
    {
        bZoneAOK = true;
    }

/* --------------------------------------------------------------------------------------- B区校验 */
    read(b_addr, buf, head_len);
    if(p_head->len > (len - head_len))
    {
        rt_kprintf("ERROR! %s Len = %d, RAM Len = %d\n", desc, p_head->len, len);
        p_head->len = len - head_len;
    }
    read(b_addr, buf, head_len+p_head->len);

    if(p_head->len > len - head_len)
    {
        p_head->len = len - head_len;
    }
    uint16_t crc16_B = crc16((uint8_t *)buf + head_len, p_head->len);
    if(crc16_B == p_head->crc16b)
    {
        bZoneBOK = true;
    }
/* --------------------------------------------------------------------------------------- AB区综合校验 */
    if(bZoneAOK && bZoneBOK)
    {
        if (crc16_A == crc16_B)
        {
            // A OK, B OK，且相同
            rt_kprintf("%s Ver:%d, A OK , B OK , A==B\n", desc, p_head->ver);
        }
        else
        {
            // A OK, B OK，但不同，以A为准
            rt_kprintf("%s Ver:%d, A OK , B OK , A!=B\n", desc, p_head->ver);
            read(a_addr, buf, len);
            write(b_addr, buf, len);
        }
    }
    else if (bZoneAOK && !bZoneBOK)
    {
        // A OK, B 不OK，将A区覆盖B区
        rt_kprintf("%s Ver:%d, A OK , B ERROR\n", desc, p_head->ver);
        read(a_addr, buf, len);
        write(b_addr, buf, len);
    }
    else if (!bZoneAOK && bZoneBOK)
    {
        // A 不OK, B OK，将B区覆盖A区
        rt_kprintf("%s Ver:%d, A ERROR , B OK\n", desc, p_head->ver);
        read(b_addr, buf, len);
        write(a_addr, buf, len);
    }
    else
    {
        // 两区均不一致
        rt_kprintf("%s A ERROR , B ERROR\n", desc);
        return 1;
    }
    return 0;

}

/* 通用AB区写接口 */
int32_t AB_save(    int32_t (*write)(uint32_t, void *, uint32_t),
                uint32_t a_addr,
                uint32_t b_addr,
                void     *buf,
                uint32_t ver,
                uint32_t len,
                char     *desc)
{
    // 写双份，以免掉电时写入未完成导致CRC校验错误
    rcd_head *p_head = buf;
    p_head->ver = ver;
    p_head->len = len - head_len;
    p_head->crc16b = crc16((uint8_t *)buf + sizeof(rcd_head), p_head->len);
    write(a_addr, buf, len);
    write(b_addr, buf, len);
    rt_kprintf("%s save success\n", desc);
    return 0;
}

