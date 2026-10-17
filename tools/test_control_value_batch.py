"""Exercise the actual five-control callback with host stubs, without device access."""
import argparse
from pathlib import Path
import subprocess
import tempfile

STUBS = r'''
#include <stdint.h>
#include <string.h>
#include <assert.h>
typedef int rt_err_t;
typedef int rt_bool_t;
typedef int Inv_Control_Type_t;
typedef int Inv_RealtimeValue_t;
typedef struct { uint8_t decimal_places; } Inv_CtrlRegBlk_t;
typedef struct { uint16_t data_len; int write_min, write_max; } Dlt645PointTypeDef;
#define RT_NULL 0
#define RT_TRUE 1
#define RT_EOK 0
#define RT_EINVAL 1
#define RT_ERROR 2
#define RT_ENOSYS 3
#define INVERTER_ARCHIVE_MAX_COUNT 12
#define DLT645_VARIABLE_ALL_SELECTOR 255
#define rt_memset memset
#define RT_UNUSED(x) ((void)(x))
enum { DLT645_CONTROL_STATUS_SUCCESS, DLT645_CONTROL_STATUS_NOT_CONTROLLED,
       DLT645_CONTROL_STATUS_FAILED, DLT645_CONTROL_STATUS_UNSUPPORTED, DLT645_CONTROL_STATUS_OTHER };
static int source_errors[12], results[12], sent[12];
static Inv_CtrlRegBlk_t reg;
static Inv_RealtimeValue_t value;
static int dlt645_field_is_ff(const uint8_t *p, int n) {
    while(n--) if(*p++ != 255) return 0;
    return 1;
}
static int dlt645_point_decimal_places(const Dlt645PointTypeDef *p, uint8_t *d) {
    (void)p; *d=0; return 0;
}
static int dlt645_get_control_source(uint32_t id, int i, Inv_Control_Type_t *t,
        const Inv_CtrlRegBlk_t **r, Inv_RealtimeValue_t **s) {
    (void)id; *t=0; *r=&reg; *s=&value; return source_errors[i];
}
static int dlt645_bcd_decode_s32(const uint8_t *p, int n, int32_t *v) {
    (void)n; if(*p==0xFA) return 1; *v=*p; return 0;
}
static int dlt645_rescale_value(int32_t v, int a, int b, int32_t *out) {
    (void)a; (void)b; *out=v; return 0;
}
static int dlt645_control_one_value(int i, int t, int v) {
    (void)t; (void)v; sent[i]++; return results[i];
}
static void reset(void) {
    memset(source_errors,0,sizeof(source_errors));
    memset(results,0,sizeof(results)); memset(sent,0,sizeof(sent));
}
'''

CASES = r'''
int main(void) {
    uint8_t data[48], response[12]; uint16_t len;
    for(int kind=5;kind<=9;kind++) {
        Dlt645PointTypeDef point={kind<=6?4:2,-1100,1100};
        uint32_t id=0x04E600FFU | ((uint32_t)kind<<8);
        reset(); memset(data,1,sizeof(data)); len=99;
        assert(dlt645_write_control_value(&point,id,data,point.data_len*12,response,12,&len)==0);
        assert(len==0); for(int i=0;i<12;i++) assert(sent[i]==1);
        reset(); results[0]=2; results[4]=3; results[9]=4; len=99;
        assert(dlt645_write_control_value(&point,id,data,point.data_len*12,response,12,&len)!=0);
        assert(len==0); for(int i=0;i<12;i++) assert(sent[i]==1);
        reset(); source_errors[0]=RT_ENOSYS; source_errors[4]=RT_ERROR;
        data[point.data_len*7]=0xFA;
        assert(dlt645_write_control_value(&point,id,data,point.data_len*12,response,12,&len)!=0);
        assert(!sent[0] && !sent[4] && !sent[7] && sent[11] && len==0);
        reset(); memset(data,255,sizeof(data));
        assert(dlt645_write_control_value(&point,id,data,point.data_len*12,response,12,&len)==0);
        assert(len==0); for(int i=0;i<12;i++) assert(sent[i]==0);
        reset(); data[0]=1; data[1]=0;
        assert(dlt645_write_control_value(&point,id,data,point.data_len*12,response,12,&len)==0);
        assert(sent[0]==1 && sent[11]==0 && len==0);
        reset(); memset(data,1,sizeof(data));
        assert(dlt645_write_control_value(&point,id,data,point.data_len*12-1,response,12,&len)!=0);
        for(int i=0;i<12;i++) assert(sent[i]==0);
        reset(); id=(id & 0xFFFFFF00U)|1;
        assert(dlt645_write_control_value(&point,id,data,point.data_len,response,12,&len)==0);
        assert(sent[0]==1 && !sent[1] && len==0);
        reset(); results[0]=2;
        assert(dlt645_write_control_value(&point,id,data,point.data_len,response,12,&len)!=0);
        assert(sent[0]==1 && len==0);
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
    start = source.index("rt_err_t dlt645_write_control_value(")
    end = source.index("/* 根据04E60B或04E60C取得时段控制方式", start)
    with tempfile.TemporaryDirectory(prefix="control_value_batch_") as directory:
        test, binary = Path(directory)/"batch.c", Path(directory)/"batch.exe"
        test.write_text(STUBS + source[start:end] + CASES, encoding="utf-8")
        subprocess.run([args.compiler,"-std=c99","-Wall","-Wextra",str(test),"-o",str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    print("PASS: 40 scenarios across five control types; no per-device response payload")

if __name__ == "__main__":
    main()
