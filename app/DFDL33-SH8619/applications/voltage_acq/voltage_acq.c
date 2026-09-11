#include "voltage_acq.h"
#include "drv_acq.h"
#include "drv_dma.h"
#include "user_logic_func.h"
#include "ctu_cfg.h"

#define  M_PI    3.14159265358979
#define  VOLTAGE_CALIBRATION_COEFFICIENT   356.204534

uint16_t g_voltage_rms = 0;;
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

float Averaging(float *buf, uint16_t len)
{
    float sum = 0.0f;
    for (uint16_t i = 0; i < len; i++)
    {
        sum += buf[i];
    }
    return sum / (float)len;
}

float rms_buf[50] = {0};
int rms_buf_idx = 0;
static float rms_accumulator = 0.0f;   /* RMS 累加值 */
static uint8_t rms_count = 0;           /* 采样次数计数 */
int calibration_flg = 0;

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
        float rms = calculate_rms(adc_dma_buffer, VOL_SAMPLE_POINTS);

        rms_buf[rms_buf_idx] = rms;
        rms_buf_idx++;
        if(50 == rms_buf_idx)
        {
            rms_buf_idx = 0;
        }

//        g_voltage_rms = rms * VOLTAGE_CALIBRATION_COEFFICIENT;

        rms_accumulator += rms;
        rms_count++;

        if (rms_count >= 5)
        {
            float rms_avg = rms_accumulator / 5.0f;
            if(calibration_flg){
                float average_value = Averaging(rms_buf, rms_buf_idx);
                ctu_cfg.g_vol_cal_coef = 2200 / average_value;
                calibration_flg = 0;
                extern void ctu_cfg_save(void);
                ctu_cfg_save();
            }
            g_voltage_rms = rms_avg * ctu_cfg.g_vol_cal_coef;

            rms_accumulator = 0.0f;
            rms_count = 0;
        }

        /* 空闲 500ms */
        rt_thread_mdelay(VOL_IDLE_MS);

        /* 重新启动下一轮采样 */
        start_voltage_sampling();
    }
}

uint16_t getvoltage_rms(void)
{
    return g_voltage_rms;
}

void show_voltage_rms(void)
{
    uint16_t vol_int = g_voltage_rms/10;
//    int vol_dec = (int)((g_voltage_rms - vol_int) * 100); // 2位小数
    uint16_t vol_dec = g_voltage_rms%10;
    rt_kprintf("Voltage Rms: %d.%d V\n", vol_int, vol_dec);
}
MSH_CMD_EXPORT(show_voltage_rms, show_voltage_rms);

void voltage_calibration(void)
{
    calibration_flg = 1;
}
MSH_CMD_EXPORT(voltage_calibration, voltage calibration);

