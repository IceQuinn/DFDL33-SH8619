"""Host regression of the actual switch-control callback, with device calls stubbed."""
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
typedef struct {uint16_t data_len; int write_min,write_max;} Dlt645PointTypeDef;
#define RT_TRUE 1
#define RT_FALSE 0
#define RT_EOK 0
#define RT_EINVAL 1
#define RT_ERROR 2
#define INVERTER_ARCHIVE_MAX_COUNT 12
#define DLT645_VARIABLE_ALL_SELECTOR 255
#define RT_UNUSED(x) ((void)(x))
#define rt_memset memset
static int calls[12], failures[12], decoded[12], received[12];
static const uint8_t *input;
static int dlt645_bcd_decode_u32(const uint8_t *p, int n, int32_t *v) {
    (void)n; decoded[p-input]++;
    if((*p & 15)>9 || (*p>>4)>9) return 1;
    *v=(*p>>4)*10+(*p&15); return 0;
}
static int dlt645_control_run_state(int i,int v) {calls[i]++;received[i]=v;return failures[i];}
static void reset(void) {
    memset(calls,0,sizeof(calls));memset(failures,0,sizeof(failures));
    memset(decoded,0,sizeof(decoded));memset(received,0,sizeof(received));
}
'''

CASES = r'''
int main(void) {
    Dlt645PointTypeDef p={1,0,1}; uint8_t data[12]={0}; uint16_t len=99; input=data;
    reset();
    assert(dlt645_write_run_state(&p,0x04E604FF,data,12,0,0,&len)==0 && len==0);
    for(int i=0;i<12;i++) assert(calls[i] && decoded[i] && received[i]==0);
    reset(); memset(data,1,12);
    assert(dlt645_write_run_state(&p,0x04E604FF,data,12,0,0,&len)==0);
    for(int i=0;i<12;i++) assert(calls[i] && received[i]==1);
    reset(); data[0]=2;data[5]=0xFA;data[9]=0xFF;
    assert(dlt645_write_run_state(&p,0x04E604FF,data,12,0,0,&len)!=0 && len==0);
    for(int i=0;i<12;i++) {assert(decoded[i]==(i!=9));assert(calls[i]==(i!=0 && i!=5 && i!=9));}
    reset(); memset(data,0,12);failures[0]=failures[6]=1;
    assert(dlt645_write_run_state(&p,0x04E604FF,data,12,0,0,&len)!=0);
    for(int i=0;i<12;i++) assert(calls[i]);
    reset();memset(data,0xFF,12);
    assert(dlt645_write_run_state(&p,0x04E604FF,data,12,0,0,&len)==0 && len==0);
    for(int i=0;i<12;i++) assert(!decoded[i] && !calls[i]);
    reset();data[0]=0;data[11]=1;
    assert(dlt645_write_run_state(&p,0x04E604FF,data,12,0,0,&len)==0 && len==0);
    for(int i=0;i<12;i++) assert(calls[i]==(i==0 || i==11));
    assert(received[0]==0 && received[11]==1);
    reset();data[0]=0xFF;
    assert(dlt645_write_run_state(&p,0x04E60403,data,1,0,0,&len)==0 && len==0);
    for(int i=0;i<12;i++) assert(!decoded[i] && !calls[i]);
    reset();
    assert(dlt645_write_run_state(&p,0x04E604FF,data,11,0,0,&len)!=0);
    for(int i=0;i<12;i++) assert(!decoded[i] && !calls[i]);
    reset();data[0]=1;
    assert(dlt645_write_run_state(&p,0x04E60403,data,1,0,0,&len)==0);
    assert(calls[2] && received[2]==1 && !calls[0] && len==0);
    reset();data[0]=2;
    assert(dlt645_write_run_state(&p,0x04E60403,data,1,0,0,&len)!=0);
    assert(!calls[2] && len==0);
    return 0;
}
'''

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", default="gcc")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    source = (root / "app/DFDL33-SH8619/applications/dlt645/dlt645_data_api.c").read_text(encoding="utf-8")
    start = source.index("rt_err_t dlt645_write_run_state(")
    end = source.index("\n}", start) + 2
    with tempfile.TemporaryDirectory(prefix="run_state_batch_") as directory:
        test, binary = Path(directory)/"batch.c", Path(directory)/"batch.exe"
        test.write_text(STUBS + source[start:end] + CASES, encoding="utf-8")
        subprocess.run([args.compiler,"-std=c99","-Wall","-Wextra",str(test),"-o",str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    print("PASS: 10 switch-control scenarios, including FF no-control, invalid values and partial execution")

if __name__ == "__main__":
    main()
