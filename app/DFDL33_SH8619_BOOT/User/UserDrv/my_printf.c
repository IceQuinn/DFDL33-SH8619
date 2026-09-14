#include "my_printf.h"
#include "my_vsnprintf.h"
#include "my_drv.h"

#define TX_BUF_LEN  256			//发送缓冲区容量，根据需要进行调整

uint8_t TxBuf[TX_BUF_LEN];		//发送缓冲区
uint8_t g_Send_Flg = 0;


void MyPrintf(const char *__format, ...)
{
	int len;
	va_list ap;
	va_start(ap, __format);
  
	/* 清空发送缓冲区 */
//  memset(TxBuf, 0x0, TX_BUF_LEN);
  
	/* 填充发送缓冲区 */
	my_vsnprintf((char*)TxBuf, TX_BUF_LEN, (const char *)__format, ap);
	va_end(ap);
	len = strlen((const char*)TxBuf);
  
	/* 往串口发送数据 */
//  HAL_UART_Transmit_DMA(&huart2, (uint8_t*)&TxBuf, len);
	UART_Transmit(USART2, (uint8_t*)&TxBuf, len);
}


//uint8_t aa;
//void USART2_IRQHandler(void)
//{
//	if(usart_flag_get(USART2, USART_TDC_FLAG))
//	{
//		usart_flag_clear(USART2, USART_TDC_FLAG);
//		aa++;
//	}
//}


void show_arr(char* name, void* data, int len)
{
    MyPrintf("[%08d]:", AT32_GetTick());

    char * ptr = (char *)data;
    MyPrintf("%s[%d] = 0x", name, len);
    for(int i=0; i<len; i++)
        MyPrintf("%02x ", ptr[i]);
    MyPrintf("\r\n");

    return;
}
