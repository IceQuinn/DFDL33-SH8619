"""Host regression test of the actual firmware batch callback with mocked dependencies.

Run: python tools/test_time_ctrl_batch.py --compiler D:/APP/mingw64/bin/gcc.exe
Only temporary build files are created; no device or Flash is accessed.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile


STUBS = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef int rt_err_t;
typedef int rt_bool_t;
typedef struct { uint16_t data_len; } Dlt645PointTypeDef;
typedef struct { uint8_t archive_index; } Time_Ctrl_Command_t;
#define RT_NULL 0
#define RT_TRUE 1
#define RT_FALSE 0
#define RT_EOK 0
#define RT_ERROR 1
#define RT_EINVAL 2
#define TIME_CTRL_RESULT_OK 0
#define INVERTER_ARCHIVE_MAX_COUNT 12
#define DLT645_VARIABLE_ALL_SELECTOR 255
#define RT_UNUSED(x) ((void)(x))
#define rt_memset memset
#define rt_kprintf(...) ((void)0)
static int present[12], decode_bad[12], check_bad[12], set_bad[12];
static int checked[12], decoded[12], attempted[12], saved[12];
static int begin_bad, end_bad, begins, ends;
static void *Inv_Data_Get(int i) { return present[i] ? &present[i] : 0; }
static int dlt645_field_is_ff(const uint8_t *p, int n) {
    while(n--) if(*p++ != 255) return 0;
    return 1;
}
static int dlt645_decode_time_control_block(uint32_t id, uint8_t i,
        const uint8_t *block, Time_Ctrl_Command_t *cmd) {
    (void)id; (void)block;
    decoded[i]++; cmd->archive_index = i; return decode_bad[i];
}
static int Time_Ctrl_Check(const Time_Ctrl_Command_t *cmd) {
    checked[cmd->archive_index]++; return check_bad[cmd->archive_index];
}
static int Time_Ctrl_Set(Time_Ctrl_Command_t cmd) {
    int i = cmd.archive_index; attempted[i]++;
    if(set_bad[i]) return 1;
    saved[i] = 1; return 0;
}
static int Time_Ctrl_Init_Batch_Begin(void) { begins++; return begin_bad; }
static int Time_Ctrl_Init_Batch_End(void) { ends++; return end_bad; }
static void reset(void) {
    for(int i=0;i<12;i++) {
        present[i]=1; decode_bad[i]=check_bad[i]=set_bad[i]=0;
        checked[i]=decoded[i]=attempted[i]=saved[i]=0;
    }
    begin_bad=end_bad=begins=ends=0;
}
'''

CASES = r'''
int main(void) {
    uint8_t data[192] = {0}; uint16_t len;
    Dlt645PointTypeDef point;
    for(int size=12;size<=16;size+=4) {
        point.data_len=size;
        reset();
        assert(dlt645_write_time_control(&point, 255, data, size*12, 0, 0, &len)==0);
        for(int i=0;i<12;i++) assert(saved[i] && checked[i]);
        assert(len==0 && begins==1 && ends==1);
        reset(); decode_bad[0]=1; check_bad[5]=1; present[7]=0; set_bad[9]=1;
        assert(dlt645_write_time_control(&point,255,data,size*12,0,0,&len)!=0);
        assert(decoded[11] && attempted[11] && saved[11] && ends==1);
        assert(!saved[0] && !saved[5] && !saved[7] && !saved[9]);
        assert(saved[1] && saved[6] && saved[8] && saved[10]);
        reset(); memset(data,255,sizeof(data)); present[4]=0;
        assert(dlt645_write_time_control(&point,255,data,size*12,0,0,&len)==0);
        assert(!decoded[4] && !attempted[4] && saved[11]);
        reset(); memset(data,0,sizeof(data)); end_bad=1;
        assert(dlt645_write_time_control(&point,255,data,size*12,0,0,&len)!=0);
        for(int i=0;i<12;i++) assert(saved[i]);
        reset();
        assert(dlt645_write_time_control(&point,255,data,size*12-1,0,0,&len)!=0);
        assert(!begins && !decoded[0] && !saved[0]);
        reset(); decode_bad[0]=1;
        assert(dlt645_write_time_control(&point,1,data,size,0,0,&len)!=0);
        assert(!begins && !saved[0]);
        reset();
        assert(dlt645_write_time_control(&point,1,data,size,0,0,&len)==0);
        assert(saved[0] && !saved[1] && !begins && !ends);
    }
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", default="gcc")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    source = (root / "app/DFDL33-SH8619/applications/dlt645/dlt645_data_api.c").read_text(encoding="utf-8")
    start = source.index("rt_err_t dlt645_write_time_control(")
    end = source.index("/* 读取DI0指定逆变器或全部逆变器的推导运行状态", start)
    with tempfile.TemporaryDirectory(prefix="time_ctrl_batch_") as directory:
        test = Path(directory) / "batch.c"
        binary = Path(directory) / "batch.exe"
        test.write_text(STUBS + source[start:end] + CASES, encoding="utf-8")
        subprocess.run([args.compiler, "-std=c99", "-Wall", "-Wextra", str(test), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    print("PASS: 14 callback scenarios (12/16-byte plans), including partial failures and single-device compatibility")


if __name__ == "__main__":
    main()
