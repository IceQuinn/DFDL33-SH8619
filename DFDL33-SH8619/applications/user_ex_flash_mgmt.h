/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-10-26     IP155       the first version
 */
#ifndef APPLICATIONS_USER_EX_FLASH_MGMT_H_
#define APPLICATIONS_USER_EX_FLASH_MGMT_H_

//总扇数， 外部FLASH大小
#define EXTFLASH_TOTAL_SIZE     (8*1024*1024)
#define EXTFLASH_PAGE_SIZE      (4096)
#define EXTFLASH_PAGE_NUM_MAX   (EXTFLASH_TOTAL_SIZE/EXTFLASH_PAGE_SIZE)    //共2048个扇

//--------------------------------------------------------------------------------  协议库 --------------------------------
#define CTU_CFG_ADDR_A                    (0      * EXTFLASH_PAGE_SIZE)
#define CTU_CFG_ADDR_B                    (1      * EXTFLASH_PAGE_SIZE)

#define PROTO_LIB_ADDR_A                  (13     * EXTFLASH_PAGE_SIZE)
#define PROTO_LIB_ADDR_B                  (19     * EXTFLASH_PAGE_SIZE)



#endif /* APPLICATIONS_USER_EX_FLASH_MGMT_H_ */
