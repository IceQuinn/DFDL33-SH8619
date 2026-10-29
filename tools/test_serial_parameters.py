"""Verify real firmware baud whitelist and seven-port 645 read/write callbacks."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile

from test_time_ctrl_archive_clear import function, ROOT

STUBS = r'''
#include <stdint.h>
#include <string.h>
#include <assert.h>
typedef int rt_err_t;
typedef struct {uint16_t data_len;} Dlt645PointTypeDef;
#define RT_NULL 0
#define RT_EOK 0
#define RT_EINVAL 1
#define RT_UNUSED(x) ((void)(x))
#define countof(a) (sizeof(a)/sizeof((a)[0]))
#define DLT645_SERIAL_PARAMETER_LEN 5
enum {UART1_NO,UART2_NO,UART3_NO,UART4_NO,UART5_NO,UART6_NO,UART7_NO,UART8_NO};
static struct {uint32_t uart_baud[8];uint16_t uart_check[8];} ctu_cfg;
static int saves;
static void ctu_cfg_save(void) {saves++;}
'''
CASES = r'''
int main(void) {
    Dlt645PointTypeDef p={5};uint16_t len;
    const uint32_t rates[]={2400,4800,9600,19200,38400,115200};
    const int ports[]={UART3_NO,UART6_NO,UART7_NO,UART5_NO,UART4_NO,UART1_NO,UART8_NO};
    for(int r=0;r<6;r++) for(int port=0;port<7;port++) for(int check=1;check<=3;check++) {
        uint32_t baud=rates[r];
        uint8_t data[]={baud,baud>>8,baud>>16,baud>>24,check},out[5];
        assert(baud_check(baud)==0);
        assert(dlt645_write_serial_parameter(&p,0x04000800+port,data,5,0,0,&len)==0 && len==0);
        assert(ctu_cfg.uart_baud[ports[port]]==baud && ctu_cfg.uart_check[ports[port]]==check);
        assert(dlt645_read_serial_parameter(&p,0x04000800+port,out,5,&len)==0 && len==5);
        assert(!memcmp(data,out,5));
    }
    assert(saves==126 && ctu_cfg.uart_baud[UART2_NO]==0);
    assert(baud_check(1200)!=0 && baud_check(57600)!=0 && baud_check(230400)!=0);
    uint8_t data[]={0,0xC2,1,0,4};
    assert(dlt645_write_serial_parameter(&p,0x04000800,data,5,0,0,&len)!=0 && saves==126);
    data[4]=1;
    assert(dlt645_write_serial_parameter(&p,0x04000807,data,5,0,0,&len)!=0 && saves==126);
    assert(dlt645_write_serial_parameter(&p,0x04000800,data,4,0,0,&len)!=0 && saves==126);
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", default="gcc")
    args = parser.parse_args()
    cfg = (ROOT / "ctu_cfg/ctu_cfg.c").read_text(encoding="utf-8")
    table = re.search(r"uint32_t baud_table\[\]\[2\] = .*?;", cfg).group()
    code = STUBS + table + function("ctu_cfg/ctu_cfg.c", "int baud_check(")
    for signature in ("static rt_err_t dlt645_serial_port_to_uart(",
                      "rt_err_t dlt645_read_serial_parameter(", "rt_err_t dlt645_write_serial_parameter("):
        code += function("dlt645/dlt645_data_api.c", signature)
    with tempfile.TemporaryDirectory(prefix="serial_parameters_") as directory:
        source, binary = Path(directory)/"test.c", Path(directory)/"test.exe"
        source.write_text(code+CASES, encoding="utf-8")
        subprocess.run([args.compiler, "-std=c99", "-Wall", "-Wextra", str(source), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    print("PASS: 126 serial parameter roundtrips, unsupported values and invalid requests")


if __name__ == "__main__":
    main()
