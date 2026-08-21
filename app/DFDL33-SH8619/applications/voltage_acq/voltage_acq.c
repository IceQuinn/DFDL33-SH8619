#include "voltage_acq.h"
#include "drv_acq.h"
#include "drv_dma.h"
#include "user_logic_func.h"

#define  M_PI    3.14159265358979

float    g_voltage_rms = 0.0f;
static rt_sem_t  g_adc_sem = RT_NULL;

/* ================================================================== */
/*                    DMA 中断服务函数                                  */
/* ================================================================== */
void DMA1_Channel1_IRQHandler(void)
{
    rt_interrupt_enter();

    /* 传输完成 */
    if (dma_interrupt_flag_get(DMA1_FDT1_FLAG) == SET)
    {
        dma_flag_clear(DMA1_FDT1_FLAG);

        tmr_counter_enable(TMR3, FALSE);

        if (g_adc_sem != RT_NULL)
        {
            rt_sem_release(g_adc_sem);
        }
    }

    rt_interrupt_leave();
}

/* ================================================================== */
/*              启动/重启一次采样                                       */
/* ================================================================== */
void start_voltage_sampling(void)
{
    dma_channel_enable(DMA1_CHANNEL1, FALSE);

    dma_data_number_set(DMA1_CHANNEL1, VOL_SAMPLE_POINTS);

    dma_flag_clear(DMA1_FDT1_FLAG);

    dma_channel_enable(DMA1_CHANNEL1, TRUE);

    adc_enable(ADC1, FALSE);
    adc_enable(ADC1, TRUE);

    tmr_counter_value_set(TMR3, 0);
    tmr_counter_enable(TMR3, TRUE);
}

void voltage_acq_thread_entry(void *param)
{
    rt_kprintf("[VOL] Voltage acquisition thread started.\n");

    /* 创建信号量 */
    g_adc_sem = rt_sem_create("adc_sem", 0, RT_IPC_FLAG_FIFO);
    if (g_adc_sem == RT_NULL)
    {
        rt_kprintf("[VOL] ERROR: sem create failed!\n");
        return;
    }

    /* 启动第一次采样 */
    start_voltage_sampling();

    while (1)
    {
        rt_sem_take(g_adc_sem, RT_WAITING_FOREVER);

        /* 计算有效值 */
        g_voltage_rms = calculate_rms(adc_dma_buffer, VOL_SAMPLE_POINTS);

//        int rms_int = (int)g_voltage_rms;
//        int rms_dec = (int)((g_voltage_rms - rms_int) * 1000);  // 取3位小数
//
//        rt_kprintf("[VOL] Voltage RMS = %d.%03d V\n", rms_int, rms_dec);

        /* 空闲 500ms */
        rt_thread_mdelay(VOL_IDLE_MS);

        /* 重新启动下一轮采样 */
        start_voltage_sampling();
    }
}


