#include "voltage_monitoring.h"
#include <rtthread.h>
#include "event_data_deal.h"

#define     RELAY_MIN_T_DELAY       30          //保护最小保持时间
#define     OVER_RETURN_COE         0.9

enum LOGIC_STAT
{
    LOGIC_OFF = 0,
    LOGIC_ON = 1,
};

//延时动作
int Inst_DeferredAction(uint8_t act_in, uint8_t* act_out, uint32_t* tick, uint32_t deferredtime)
{
    if(RT_NULL == act_out || RT_NULL == tick)
    {
        return RT_ERROR;
    }
    if(LOGIC_ON == act_in)
    {
        if((rt_tick_get()-(*tick)) >= deferredtime)
        {
            *act_out = LOGIC_ON;
        }
    }
    else
    {
        *tick = rt_tick_get();
        *act_out = LOGIC_OFF;

    }
    return RT_EOK;
}

uint8_t chkLogic( uint8_t byVal )
{
    if ( LOGIC_OFF == byVal )
    {
        return 0;
    }
    return 1;
}
//非门
int NotLogic( uint8_t* o_pbyVal, uint8_t* i_pbyVal )
{
    if ( RT_NULL == i_pbyVal || RT_NULL == o_pbyVal )
    {
        return RT_ERROR;
    }

    *o_pbyVal = chkLogic(*i_pbyVal) ? 0 : 1;
    return RT_EOK;
}

//或门
int OrLogic( uint8_t* o_pbyV, uint8_t* i_pbyV1, uint8_t* i_pbyV2 )
{
    if ( (RT_NULL == o_pbyV) || (RT_NULL == i_pbyV1) || (RT_NULL == i_pbyV2) )
    {
        return RT_ERROR;
    }

    *o_pbyV = ((*i_pbyV1) || (*i_pbyV2)) ? 1: 0;
    return RT_EOK;
}
//或非门
int NotOrLogic( uint8_t* o_pbyV, uint8_t* i_pbyV1, uint8_t* i_pbyV2 )
{
    int nRet = OrLogic( o_pbyV, i_pbyV1, i_pbyV2 );
    if ( RT_EOK != nRet )
    {
        return nRet;
    }
    return NotLogic( o_pbyV, o_pbyV );
}

//SR锁存器
//一般只使用参数1(o_pbySet)作为输出
int SRlatchLogic( uint8_t* o_pbySet, uint8_t* i_pbySet, uint8_t* i_pbyReset )
{
    uint8_t tmpSet = LOGIC_OFF;
    int nRet = NotOrLogic( &tmpSet, i_pbySet, o_pbySet );
    if ( RT_EOK != nRet )
    {
        return nRet;
    }
    nRet = NotOrLogic( o_pbySet, i_pbyReset, &tmpSet );
    if ( RT_EOK != nRet )
    {
        return nRet;
    }
    return RT_EOK;
}

VoltageOverLimit_t VoltageOverData;
/* 电压越限检测 */
int over_voltage_check(over_voltage *p_over_voltage)
{
    uint32_t u_a = *p_over_voltage->u_a;
//    uint32_t u_b = *p_over_voltage->u_b;
//    uint32_t u_c = *p_over_voltage->u_c;

    uint32_t setting = *p_over_voltage->u_oversetting;
    uint32_t t_setting = RELAY_MIN_T_DELAY;
    if(*p_over_voltage->t_setting > t_setting)
        t_setting = *p_over_voltage->t_setting;

    uint16_t event = p_over_voltage->event;

//--------------------------------------------------------Ua------------------------------------------------------------------
    uint8_t  x_ua = (u_a > setting);
    static uint8_t  y_ua = 0;
    static uint32_t tick_ua = 0;
    /* 过门槛持续指定时间 */
    Inst_DeferredAction(x_ua, &y_ua, &tick_ua, t_setting);
    uint8_t r_ua = (u_a < (setting * OVER_RETURN_COE));
    static uint8_t sr_out_ua = 0;
    SRlatchLogic(&sr_out_ua, &y_ua, &r_ua);

////--------------------------------------------------------Ub------------------------------------------------------------------
//    uint8_t  x_ub = ((u_b > setting) && (*p_over_voltage->b_enable));
//    static uint8_t  y_ub = 0;
//    static uint32_t tick_ub = 0;
//    /* 过门槛持续指定时间 */
//    Inst_DeferredAction(x_ub, &y_ub, &tick_ub, t_setting);
//    uint8_t r_ub = ((u_b < (setting * OVER_RETURN_COE)) || (!*p_over_voltage->b_enable));
//    static uint8_t sr_out_ub = 0;
//    SRlatchLogic(&sr_out_ub, &y_ub, &r_ub);
//
////--------------------------------------------------------Uc------------------------------------------------------------------
//    uint8_t  x_uc = ((u_c > setting) && (*p_over_voltage->b_enable));
//    static uint8_t  y_uc = 0;
//    static uint32_t tick_uc = 0;
//    /* 过门槛持续指定时间 */
//    Inst_DeferredAction(x_uc, &y_uc, &tick_uc, t_setting);
//    uint8_t r_uc = ((u_c < (setting * OVER_RETURN_COE)) || (!*p_over_voltage->b_enable));
//    static uint8_t sr_out_uc = 0;
//    SRlatchLogic(&sr_out_uc, &y_uc, &r_uc);

    uint8_t act_v = 0;

//    if( (sr_out_ua || sr_out_ub || sr_out_uc))
    if(sr_out_ua)
    {
        act_v = 1;
    }
    if(act_v)
    {
        if(1 != *p_over_voltage->fault_status)
        {   /* 上一状态：未触发 */
            *p_over_voltage->fault_status = 1;

            Get_EventTime(&VoltageOverData.start_time);
            VoltageOverData.over_limit_vol_a = u_a;
            VoltageOverData.over_limit_vol_b = 0;
            VoltageOverData.over_limit_vol_c = 0;
        }
        /* 上一状态：触发中 */
    }
    else
    {
        if(0 != *p_over_voltage->fault_status)
        {   /* 上一状态：触发中 */
            *p_over_voltage->fault_status = 0;
            Get_EventTime(&VoltageOverData.start_time);
            VoltageOverData.recover_vol_a = u_a;
            VoltageOverData.recover_vol_b = 0;
            VoltageOverData.recover_vol_c = 0;
            Send_Event_Data_To_Cache_By_Type(event, &VoltageOverData);
        }
    }
    return RT_EOK;
}



