/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-09-11     mutou       the first version
 */
#include "event_cfg.h"
#include "sys.h"


struct event_desc_map event_desc[] =
{
//系统事件
{ETP_REBOOT,                        EVT_CLASS_OPERATE,                  TRUE,    0, "装置重启"},
{ETP_POWER_DOWN,                    EVT_CLASS_OPERATE,                  TRUE,    0, "装置失电"},
{ETP_POWER_UP,                      EVT_CLASS_OPERATE,                  TRUE,    0, "装置上电"},
{ETP_SCAP_ERR,                      EVT_CLASS_SELF_CHECK,               TRUE,    0, "超级电容故障"},

};

uint32_t Event_Table_Size = sizeof(event_desc);


const struct event_desc_map* get_event_desc(uint16_t evt_id)
{
    for(int i=0; i<countof(event_desc); i++)
    {
        if(event_desc[i].evt_id == evt_id)
            return &event_desc[i];
    }
    return RT_NULL;
}
