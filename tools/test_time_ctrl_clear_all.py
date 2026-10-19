"""Check all-plan erasure and real 645 factory-reset dispatch with mocked hardware."""
import argparse
from pathlib import Path
import subprocess
import tempfile

from test_time_ctrl_archive_clear import COMMON, STORE, RUNTIME, function


STORE_CASES = r'''
static void seed(void) {
    memset(&g_time_ctrl_store,1,sizeof(g_time_ctrl_store));
}
static void check_empty(void) {
    for(int i=0;i<12;i++) {
        Time_Ctrl_Store_Entry_t empty={0};empty.command.archive_index=i;
        assert(!memcmp(&empty,&g_time_ctrl_store.entries[i],sizeof(empty)));
    }
}
int main(void) {
    seed();
    assert(Time_Ctrl_Init_Clear_All()==0 && saves==1 && !g_time_ctrl_store_dirty && depth==0);
    check_empty();
    assert(Time_Ctrl_Init_Clear_All()==0 && saves==1);
    seed();save_error=9;
    assert(Time_Ctrl_Init_Clear_All()==9 && saves==2 && g_time_ctrl_store_dirty && depth==0);
    check_empty();
    save_error=0;
    assert(Time_Ctrl_Init_Clear_All()==0 && saves==3 && !g_time_ctrl_store_dirty);
    seed();g_time_ctrl_store_batch_depth=1;
    assert(Time_Ctrl_Init_Clear_All()==-RT_EBUSY && saves==3 && depth==0);
    assert(g_time_ctrl_store.entries[0].enabled==1);
    g_time_ctrl_store_batch_depth=0;
    assert(Time_Ctrl_Init_Clear_All()==0 && saves==4);check_empty();
    return 0;
}
'''

RUNTIME_CASES = r'''
static void seed(void) {
    memset(g_time_ctrl_mailboxes,1,sizeof(g_time_ctrl_mailboxes));
    memset(g_time_ctrl_contexts,1,sizeof(g_time_ctrl_contexts));
    memset(&g_time_ctrl_store,1,sizeof(g_time_ctrl_store));
    for(int i=0;i<12;i++) g_time_ctrl_contexts[i].action_request_id=i+1;
}
static void check_empty(void) {
    for(int i=0;i<12;i++) {
        Time_Ctrl_Context_t empty={0};empty.command.archive_index=i;
        empty.generation=g_time_ctrl_mailboxes[i].generation;
        empty.active_period=empty.action_period=-1;
        assert(!memcmp(&empty,&g_time_ctrl_contexts[i],sizeof(empty)));
        assert(!g_time_ctrl_mailboxes[i].enabled);
        Time_Ctrl_Command_t command={0};command.archive_index=i;
        assert(!memcmp(&command,&g_time_ctrl_mailboxes[i].command,sizeof(command)));
    }
}
int main(void) {
    seed();
    assert(Time_Ctrl_Clear_All()==0 && cancel_calls==12 && cancelled==12 && depth==0 && saves==1);
    check_empty();
    seed();save_error=9;
    assert(Time_Ctrl_Clear_All()==9 && depth==0 && g_time_ctrl_store_dirty);
    check_empty();
    save_error=0;
    assert(Time_Ctrl_Clear_All()==0 && !g_time_ctrl_store_dirty && depth==0);
    check_empty();
    return 0;
}
'''

ACTION = r'''
#define RT_ERROR 3
#define RT_UNUSED(x) ((void)(x))
#define EVT_CLASS_MAX 7
typedef struct {uint16_t data_len;} Dlt645PointTypeDef;
static int step,clear_error,clear_calls,archives,defaults,reboots,events,calibrations;
static int dlt645_bcd_decode_u32(const uint8_t *p,uint16_t n,int32_t *v) {
    (void)n;if((*p&15)>9 || (*p>>4)>9)return -1;*v=(*p>>4)*10+(*p&15);return 0;
}
static int Time_Ctrl_Clear_All(void) {clear_calls++;assert(step==0);step=1;return clear_error;}
static void Inv_Archive_Default_Init(void) {assert(step==1);step=2;archives++;}
static void set_default_data(void) {assert(step==2);step=3;defaults++;}
static void reboot(void) {reboots++;}
static void Clear_Events(int category) {(void)category;events++;}
static void voltage_calibration(void) {calibrations++;}
'''

ACTION_CASES = r'''
int main(void) {
    Dlt645PointTypeDef point={1};uint8_t data=1;uint16_t length=99;
    assert(dlt645_write_device_action(&point,0x04000C00,&data,1,0,0,&length)==0);
    assert(step==3 && clear_calls==1 && archives==1 && defaults==1 && reboots==1 && length==0);
    step=0;clear_error=9;
    assert(dlt645_write_device_action(&point,0x04000C00,&data,1,0,0,&length)==-RT_ERROR);
    assert(step==1 && clear_calls==2 && archives==1 && defaults==1 && reboots==1);
    data=0;
    assert(dlt645_write_device_action(&point,0x04000C00,&data,1,0,0,&length)!=0 && clear_calls==2);
    data=1;
    assert(dlt645_write_device_action(&point,0x04000C00,&data,0,0,0,&length)!=0 && clear_calls==2);
    assert(dlt645_write_device_action(&point,0x04000A00,&data,1,0,0,&length)==0 && reboots==2);
    assert(dlt645_write_device_action(&point,0x04000B00,&data,1,0,0,&length)==0 && events==1);
    assert(dlt645_write_device_action(&point,0x04000D00,&data,1,0,0,&length)==0 && calibrations==1);
    assert(clear_calls==2);
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", default="gcc")
    args = parser.parse_args()
    store = function("cycle_loop/time_ctrl_init.c", "rt_err_t Time_Ctrl_Init_Clear_All(")
    runtime = function("cycle_loop/time_ctrl.c", "rt_err_t Time_Ctrl_Clear_All(")
    action = function("dlt645/dlt645_data_api.c", "rt_err_t dlt645_write_device_action(")
    with tempfile.TemporaryDirectory(prefix="time_ctrl_clear_all_") as directory:
        for name, code in (("store", STORE+store+STORE_CASES),
                           ("runtime", RUNTIME+store+runtime+RUNTIME_CASES),
                           ("645_factory_reset", ACTION+action+ACTION_CASES)):
            source, binary = Path(directory)/f"{name}.c", Path(directory)/f"{name}.exe"
            source.write_text(COMMON+code, encoding="utf-8")
            subprocess.run([args.compiler, "-std=c99", "-Wno-unused-function", str(source), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)
            print(f"PASS: {name}")


if __name__ == "__main__":
    main()
