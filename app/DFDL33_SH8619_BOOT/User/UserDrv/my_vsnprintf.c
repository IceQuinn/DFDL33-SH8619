#include "my_vsnprintf.h"


static void buffer_putc(BufferState* s, char c) {
    if (s->count < s->size - 1) {
        s->buf[s->count] = c;
    }
    s->count++;
}


// 整数转字符串（支持符号、进制、大小写）
static char* format_integer(char* buf, unsigned long num, int base, 
                           int width, int precision, int flags) {
    char tmp[32];
    char sign = 0;
    int  i = 0, len = 0;
    
    const char* digits = (flags & UPPERCASE) ? 
        "0123456789ABCDEF" : "0123456789abcdef";
    
    // 处理符号
	//if (flags & SIGNED && num < 0)
    if(flags & SIGNED )
	{
		long signed_num = (long)num;
		if(signed_num < 0)
		{
			sign = '-';
			num = -(unsigned long)(signed_num);
		}
    } 
	else if(flags & SIGN_PLUS){
        sign = '+';
    } 
	else if(flags & SIGN_SPACE){
        sign = ' ';
    }
    
    // 处理前缀 (0x/0X)
    if(flags & PREFIX_HEX && base == 16)
	{
        len += 2;		// 为"0x"预留空间
    }

    // 转换数字（反向存储）
    if(num == 0)
	{
        tmp[i++] = '0';		// 处理零值特殊情况
    }
	else
	{
        while (num != 0)
		{
            tmp[i++] = digits[num % base];		// 取余获取低位数字
            num /= base;		// 整除降位
        }
    }
    
	#if 0
    // 处理精度要求（补零）
    while (i < precision) {
        tmp[i++] = '0';		// 左侧补零
    }
	#endif
    
    // 计算总长度（符号+前缀+数字）
    len += i + (sign ? 1 : 0);
    
    // 处理宽度对齐（右对齐）
    if(!(flags & ALIGN_LEFT))
	{
        char pad_char = (flags & ALIGN_ZERO) ? '0' : ' ';
        while(len < width)
		{
            *buf++ = pad_char;
            width--;
        }
    }
    
    // 写入符号
    if(sign)
        *buf++ = sign;
    
    // 写入前缀
    if(flags & PREFIX_HEX && base == 16)
	{
        *buf++ = '0';
        *buf++ = (flags & UPPERCASE) ? 'X' : 'x';
    }
    
    // 写入数字（反向纠正顺序）
    while(i > 0)
	{
        *buf++ = tmp[--i];
    }
    
    // 处理宽度对齐（左对齐）
    if(flags & ALIGN_LEFT)
	{
        while(len < width)
		{
            *buf++ = ' ';
            width--;
        }
    }
    
    return buf;
}


#if 0
// 浮点数转字符串（支持精度控制）
static char* format_float(char* buf, double num, 
                         int width, int precision, int flags)
{
    char sign = 0;
    long int_part;
    double frac_part;
    
    // 处理特殊值
    if (isnan(num)) return strcpy(buf, "nan") + 3;
    if (isinf(num)) return strcpy(buf, "inf") + 3;
    
    // 处理符号
    if(flags & SIGNED && num < 0)
	{
        sign = '-';
        num = -num;
    }
	else if(flags & SIGN_PLUS)
	{
        sign = '+';
    }
	else if(flags & SIGN_SPACE)
	{
        sign = ' ';
    }
    
    // 分离整数和小数部分
    int_part = (long)num;
    frac_part = num - int_part;
    
    // 处理整数部分
    buf = format_integer(buf, int_part, 10, 0, 1, 
                        flags | (sign ? SIGNED : 0));
    
    // 处理小数部分
    if(precision > 0)
	{
        *buf++ = '.';
        
        // 处理小数精度
        while (precision-- > 0)
		{
            frac_part *= 10;
            int digit = (int)frac_part;
            *buf++ = '0' + digit;
            frac_part -= digit;
        }
    }
    
    return buf;
}
#endif


int my_vsnprintf(char* buf, size_t size, const char* fmt, va_list args) 
{
    BufferState state = {buf, size, 0};
    int flags, width, precision;
	char tmp_buf[64];
	char* p;
	size_t len;
    
    for(; *fmt; fmt++)
	{
        if(*fmt != '%')
		{
            buffer_putc(&state, *fmt);
            continue;
        }
        
        // 解析格式标识符
        fmt++;
        flags = 0;
        width = 0;
        precision = -1;		// -1 表示未设置精度
        
        // 解析标志
        while(1)
		{
            switch(*fmt)
			{
                case '-': flags |= ALIGN_LEFT; fmt++; continue;
                case '0': flags |= ALIGN_ZERO; fmt++; continue;
                case '+': flags |= SIGN_PLUS; fmt++; continue;
                case ' ': flags |= SIGN_SPACE; fmt++; continue;
                case '#': flags |= PREFIX_HEX; fmt++; continue;
            }
            break;
        }
        
        // 解析宽度
        if(*fmt >= '0' && *fmt <= '9')
		{
            width = 0;
            while(*fmt >= '0' && *fmt <= '9')
			{
                width = width * 10 + (*fmt - '0');
                fmt++;
            }
        }
        
		#if 0
        // 解析精度
        if (*fmt == '.') {
            fmt++;
            precision = 0;
            if (*fmt >= '0' && *fmt <= '9') {
                while (*fmt >= '0' && *fmt <= '9') {
                    precision = precision * 10 + (*fmt - '0');
                    fmt++;
                }
            }
        }
		#endif
        
        // 处理特殊格式
		p = tmp_buf;
        //char* end = tmp_buf + sizeof(tmp_buf);
        
        switch(*fmt)
		{
            case 'd':
            case 'i': {
                int num = va_arg(args, int);
                p = format_integer(tmp_buf, num, 10, width, 
                                  precision, flags | SIGNED);
                break;
            }
			
			#if 0
            case 'u': {
                unsigned long num = va_arg(args, unsigned long);
                p = format_integer(tmp_buf, num, 10, width, precision, flags);
                break;
            }
			#endif
			
            case 'x': {
                unsigned int num = va_arg(args, unsigned int);
                p = format_integer(tmp_buf, (unsigned int)num, 16, width, 
                                  precision, flags);
                break;
            }
			
			#if 0
            case 'X': {
                unsigned long num = va_arg(args, unsigned long);
                p = format_integer(tmp_buf, num, 16, width, precision, 
                                  flags | UPPERCASE);
                break;
            }
            case 'f': {
                double num = va_arg(args, double);
                if (precision < 0) precision = 6; // 默认精度
                p = format_float(tmp_buf, num, width, precision, flags);
                break;
            }
			
            case 'c': {
                char c = (char)va_arg(args, int);
                tmp_buf[0] = c;
                p = tmp_buf + 1;
                break;
            }
			#endif
			
            case 's': {
                const char* s = va_arg(args, const char*);
                if(!s) s = "(null)";
                
                len = strlen(s);
                if(precision >= 0 && (size_t)precision < len)
                    len = precision;
                
                // 处理宽度对齐（右对齐：先填充空格）
                if(!(flags & ALIGN_LEFT))
				{
                    while(len < (size_t)width)
					{
                        buffer_putc(&state, ' ');
                        width--;
                    }
                }
                
                // 写入字符串
                for(size_t i = 0; i < len; i++)
				{
                    buffer_putc(&state, s[i]);
                }
                
                // 处理宽度对齐（左对齐：后填充空格）
                if(flags & ALIGN_LEFT)
				{
                    while(len < (size_t)width)
					{
                        buffer_putc(&state, ' ');
                        width--;
                    }
                }               
                continue;
            }
            case '%': {
                buffer_putc(&state, '%');
                continue;
            }
            default: {
                buffer_putc(&state, '%');
                buffer_putc(&state, *fmt);
                continue;
            }
        }
        
        // 写入格式化的内容
        for(char* q = tmp_buf; q < p; q++)
		{
            buffer_putc(&state, *q);
        }
    }
    
    // 添加终止空字符
    if(state.size > 0)
	{
        state.buf[state.count < state.size ? state.count : state.size - 1] = '\0';
    }
    
    return state.count;		// 返回写入的字符数（不包括终止符）
}
