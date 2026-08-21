#include "drv_acq.h"

#define DMA1_CHANNEL1_MEMORY_BASE_ADDR  ((uint32_t)adc_dma_buffer)
#define DMA1_CHANNEL1_BUFFER_SIZE   40


uint16_t adc_dma_buffer[VOL_SAMPLE_POINTS] = {0};

/**
  * @brief  init adc1 function.
  * @param  none
  * @retval none
  */
void wk_adc1_init(void)
{
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_ADC1_PERIPH_CLOCK, TRUE);

    gpio_init_type gpio_init_struct;
    adc_base_config_type adc_base_struct;

    gpio_default_para_init(&gpio_init_struct);

    /*gpio--------------------------------------------------------------------*/
    /* configure the IN10 pin */
    gpio_init_struct.gpio_mode = GPIO_MODE_ANALOG;
    gpio_init_struct.gpio_pins = GPIO_PINS_0;
    gpio_init(GPIOC, &gpio_init_struct);

    adc_reset(ADC1);
    crm_adc_clock_div_set(CRM_ADC_DIV_6);

    /*adc_common_settings-------------------------------------------------------------*/
    adc_combine_mode_select(ADC_INDEPENDENT_MODE);

    /*adc_settings--------------------------------------------------------------------*/
    adc_base_default_para_init(&adc_base_struct);
    adc_base_struct.sequence_mode = FALSE;
    adc_base_struct.repeat_mode = FALSE;
    adc_base_struct.data_align = ADC_RIGHT_ALIGNMENT;
    adc_base_struct.ordinary_channel_length = 1;
    adc_base_config(ADC1, &adc_base_struct);

    /* adc_ordinary_conversionmode-------------------------------------------- */
    adc_ordinary_channel_set(ADC1, ADC_CHANNEL_10, 1, ADC_SAMPLETIME_239_5);

    /* When "ADCx_ORDINARY_TRIG_SOFTWARE" is selected, user can only use software trigger. \
      The software trigger function is adc_ordinary_software_trigger_enable(ADCx, TRUE); */
    adc_ordinary_conversion_trigger_set(ADC1, ADC12_ORDINARY_TRIG_TMR3TRGOUT, TRUE);

    adc_ordinary_part_mode_enable(ADC1, FALSE);

    adc_dma_mode_enable(ADC1, TRUE);

    adc_enable(ADC1, TRUE);

    /* adc calibration-------------------------------------------------------- */
    adc_calibration_init(ADC1);
    while(adc_calibration_init_status_get(ADC1));
    adc_calibration_start(ADC1);
    while(adc_calibration_status_get(ADC1));
}

/**
  * @brief  init dma1 channel1 for "adc1"
  * @param  none
  * @retval none
  */
void wk_dma1_channel1_init(void)
{
    dma_init_type dma_init_struct;

    crm_periph_clock_enable(CRM_DMA1_PERIPH_CLOCK, TRUE);

    dma_reset(DMA1_CHANNEL1);
    dma_default_para_init(&dma_init_struct);
    dma_init_struct.direction = DMA_DIR_PERIPHERAL_TO_MEMORY;
    dma_init_struct.memory_data_width = DMA_MEMORY_DATA_WIDTH_HALFWORD;
    dma_init_struct.memory_inc_enable = TRUE;
    dma_init_struct.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_HALFWORD;
    dma_init_struct.peripheral_inc_enable = FALSE;
    dma_init_struct.priority = DMA_PRIORITY_LOW;
    dma_init_struct.loop_mode_enable = FALSE;
    dma_init(DMA1_CHANNEL1, &dma_init_struct);

    /* flexible function enable */
    dma_flexible_config(DMA1, FLEX_CHANNEL1, DMA_FLEXIBLE_ADC1);

    // 开启 DMA 传输完成中断
    dma_interrupt_enable(DMA1_CHANNEL1, DMA_FDT_INT, TRUE);

    // 使能 DMA1_Channel1 中断
    nvic_irq_enable(DMA1_Channel1_IRQn, 0, 0);
}

/**
  * @brief  init tmr3 function.
  * @param  none
  * @retval none
  */
void wk_tmr3_init(void)
{
    crm_periph_clock_enable(CRM_TMR3_PERIPH_CLOCK, TRUE);

    /* configure counter settings */
    tmr_cnt_dir_set(TMR3, TMR_COUNT_UP);
    tmr_clock_source_div_set(TMR3, TMR_CLOCK_DIV1);
    tmr_period_buffer_enable(TMR3, FALSE);
//    tmr_base_init(TMR3, 119, 999);//240MHz配置
    tmr_base_init(TMR3, 99, 899);//180MHZ配置

    /* configure primary mode settings */
    tmr_sub_sync_mode_set(TMR3, FALSE);
    tmr_primary_mode_select(TMR3, TMR_PRIMARY_SEL_OVERFLOW);

//    tmr_counter_enable(TMR3, TRUE);
}

void wk_dma_channel_config(dma_channel_type* dmax_channely, uint32_t peripheral_base_addr, uint32_t memory_base_addr, uint16_t buffer_size)
{
  dmax_channely->dtcnt = buffer_size;
  dmax_channely->paddr = peripheral_base_addr;
  dmax_channely->maddr = memory_base_addr;
}

int hw_adc_dma_tmr_init(void)
{
    wk_adc1_init();
    wk_dma1_channel1_init();
    wk_dma_channel_config(DMA1_CHANNEL1,
                          (uint32_t)&ADC1->odt,
                          DMA1_CHANNEL1_MEMORY_BASE_ADDR,
                          DMA1_CHANNEL1_BUFFER_SIZE);
    dma_channel_enable(DMA1_CHANNEL1, TRUE);
    wk_tmr3_init();
    return RT_EOK;
}
INIT_BOARD_EXPORT(hw_adc_dma_tmr_init);  // 板级初始化阶段执行



