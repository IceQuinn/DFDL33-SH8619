"""Compile real firmware functions with mocks to verify archive-related plan cleanup."""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1] / "app/DFDL33-SH8619/applications"


def function(path, signature):
    source = (ROOT / path).read_text(encoding="utf-8")
    start = source.index(signature)
    return source[start:source.index("\n}", start) + 2] + "\n"


COMMON = r'''
#include <stdint.h>
#include <string.h>
#include <assert.h>
typedef int rt_err_t;
typedef int rt_bool_t;
typedef int rt_base_t;
#define RT_EOK 0
#define RT_EINVAL 1
#define RT_TRUE 1
#define RT_FALSE 0
#define RT_WAITING_FOREVER -1
#define RT_NULL 0
#define INVERTER_ARCHIVE_MAX_COUNT 12
#define rt_memset memset
#define rt_memcpy memcpy
#define rt_memcmp memcmp
#define rt_kprintf(...) ((void)0)
static int depth;
static void rt_mutex_take(int *m,int timeout) {(void)m;(void)timeout;depth++;}
static void rt_mutex_release(int *m) {(void)m;assert(depth>0);depth--;}
static int rt_hw_interrupt_disable(void) {return 0;}
static void rt_hw_interrupt_enable(int level) {(void)level;}
'''

STORE = r'''
typedef struct {uint8_t archive_index; int periods[8];} Time_Ctrl_Command_t;
typedef struct {uint8_t enabled,restore_pending,reserved[2];Time_Ctrl_Command_t command;} Time_Ctrl_Store_Entry_t;
static struct {Time_Ctrl_Store_Entry_t entries[12];} g_time_ctrl_store;
static int g_time_ctrl_store_mutex,g_time_ctrl_store_dirty,g_time_ctrl_store_batch_depth;
static int saves,save_error;
static int time_ctrl_store_save_locked(void) {saves++;return save_error;}
'''
STORE_CASES = r'''
int main(void) {
    for(int i=0;i<12;i++) g_time_ctrl_store.entries[i].command.archive_index=i;
    memset(&g_time_ctrl_store.entries[3],1,sizeof(Time_Ctrl_Store_Entry_t));
    g_time_ctrl_store.entries[2].enabled=1;
    assert(Time_Ctrl_Init_Clear_Archive(3)==0 && saves==1 && depth==0);
    Time_Ctrl_Store_Entry_t empty={0};empty.command.archive_index=3;
    assert(!memcmp(&empty,&g_time_ctrl_store.entries[3],sizeof(empty)));
    assert(g_time_ctrl_store.entries[2].enabled==1 && !g_time_ctrl_store_dirty);
    assert(Time_Ctrl_Init_Clear_Archive(3)==0 && saves==1);
    assert(Time_Ctrl_Init_Clear_Archive(12)!=0 && saves==1);
    g_time_ctrl_store.entries[3].restore_pending=1;save_error=9;
    assert(Time_Ctrl_Init_Clear_Archive(3)==9 && g_time_ctrl_store_dirty && depth==0);
    save_error=0;
    assert(Time_Ctrl_Init_Clear_Archive(3)==0 && saves==3 && !g_time_ctrl_store_dirty);
    g_time_ctrl_store_batch_depth=1;g_time_ctrl_store.entries[3].enabled=1;
    assert(Time_Ctrl_Init_Clear_Archive(3)==0 && saves==3 && g_time_ctrl_store_dirty);
    return 0;
}
'''

RUNTIME = STORE + r'''
#define TIME_CTRL_NO_PERIOD -1
typedef struct {Time_Ctrl_Command_t command;uint32_t generation;int enabled;} Time_Ctrl_Mailbox_t;
typedef struct {Time_Ctrl_Command_t command;uint32_t generation,action_request_id;int active_period,action_period,output_limited,boot_restore_pending,retry_tick,enabled,action;} Time_Ctrl_Context_t;
static Time_Ctrl_Mailbox_t g_time_ctrl_mailboxes[12];
static Time_Ctrl_Context_t g_time_ctrl_contexts[12];
static int g_time_ctrl_runtime_mutex;
static uint32_t cancelled;
static void Inv_Control_Cancel_Queued(uint32_t id) {cancelled=id;}
'''
RUNTIME_CASES = r'''
int main(void) {
    memset(&g_time_ctrl_mailboxes[4],1,sizeof(Time_Ctrl_Mailbox_t));
    memset(&g_time_ctrl_contexts[4],1,sizeof(Time_Ctrl_Context_t));
    g_time_ctrl_contexts[4].action_request_id=123;
    g_time_ctrl_contexts[5].enabled=1;g_time_ctrl_store.entries[4].restore_pending=1;
    assert(Time_Ctrl_Archive_Change_Begin(4)==0 && depth==1 && cancelled==123);
    assert(!g_time_ctrl_mailboxes[4].enabled && !g_time_ctrl_contexts[4].enabled);
    assert(!g_time_ctrl_contexts[4].action && !g_time_ctrl_contexts[4].action_request_id);
    assert(!g_time_ctrl_contexts[4].output_limited && !g_time_ctrl_contexts[4].boot_restore_pending);
    assert(!g_time_ctrl_contexts[4].retry_tick && g_time_ctrl_contexts[4].active_period==-1);
    assert(g_time_ctrl_contexts[4].generation==g_time_ctrl_mailboxes[4].generation);
    assert(!g_time_ctrl_store.entries[4].restore_pending && g_time_ctrl_contexts[5].enabled);
    Time_Ctrl_Archive_Change_End();assert(depth==0);
    assert(Time_Ctrl_Archive_Change_Begin(12)!=0 && depth==0);
    save_error=9;g_time_ctrl_store.entries[4].enabled=1;
    assert(Time_Ctrl_Archive_Change_Begin(4)==9 && depth==0);
    assert(!g_time_ctrl_mailboxes[4].enabled && !g_time_ctrl_contexts[4].output_limited);
    return 0;
}
'''

QUEUE = r'''
#define INV_CONTROL_QUEUE_SIZE 4
#define INV_DATA_PORT_COUNT 3
typedef struct {uint32_t request_id;} Inv_Control_Request_t;
typedef struct {Inv_Control_Request_t requests[4];uint8_t read_index,write_index,count;} Inv_Control_Queue_t;
static struct {Inv_Control_Queue_t control_queue;} g_inv_data_ports[3];
'''
QUEUE_CASES = r'''
int main(void) {
    for(int start=0;start<4;start++) for(int count=0;count<=4;count++) {
        for(int removed=0;removed<=count;removed++) {
            memset(g_inv_data_ports,0,sizeof(g_inv_data_ports));
            Inv_Control_Queue_t *q=&g_inv_data_ports[0].control_queue;
            q->read_index=start;q->count=count;q->write_index=(start+count)%4;
            for(int i=0;i<count;i++) q->requests[(start+i)%4].request_id=i+1;
            Inv_Control_Cancel_Queued(removed);
            int expected=count-(removed!=0);assert(q->count==expected);
            int offset=0;
            for(int i=1;i<=count;i++) if(i!=removed) assert(q->requests[(start+offset++)%4].request_id==(uint32_t)i);
            assert(q->write_index==(start+expected)%4);
        }
    }
    return 0;
}
'''

ARCHIVE = r'''
#define INVERTER_ARCHIVE_ADD_FAILED -1
#define INVERTER_ARCHIVE_VALID 1
#define INVERTER_ARCHIVE_INVALID 0
#define INV_PORT_RJ45_1 1
#define INV_PORT_WIRELESS 4
typedef struct {int id;} Inv_Proto_t;
typedef struct {int id;} Inv_MfrInfo_t;
typedef struct {uint8_t mb_addr,port;Inv_MfrInfo_t mfr_info;} Inv_Archive_t;
static struct {Inv_Archive_t archives[12];int valid[12];} g_inv_archive_lib;
static const Inv_Proto_t *g_inv_archive_proto[12];
static Inv_Proto_t protocol;
static int begin_count,end_count,clear_error,archive_saves;
static const Inv_Proto_t *inv_archive_find_protocol(const Inv_MfrInfo_t *m) {return m->id==1?&protocol:0;}
static int Time_Ctrl_Archive_Change_Begin(uint8_t i) {(void)i;begin_count++;return clear_error;}
static void Time_Ctrl_Archive_Change_End(void) {end_count++;}
static void rt_enter_critical(void) {}
static void rt_exit_critical(void) {}
static void Inv_Archive_Save(void) {archive_saves++;}
'''
ARCHIVE_CASES = r'''
int main(void) {
    Inv_Archive_t a={1,1,{1}};
    assert(Inv_Archive_Set(0,&a)==0 && begin_count==1 && end_count==1);
    assert(Inv_Archive_Set(0,&a)==0 && begin_count==1 && archive_saves==1);
    a.mb_addr=2;
    assert(Inv_Archive_Set(0,&a)==0 && begin_count==2 && end_count==2);
    assert(Inv_Archive_Set(1,&a)==-1 && begin_count==2);
    a.mfr_info.id=9;
    assert(Inv_Archive_Set(0,&a)==-1 && begin_count==2);
    a.mfr_info.id=1;a.mb_addr=3;clear_error=9;
    assert(Inv_Archive_Set(0,&a)==-1 && g_inv_archive_lib.archives[0].mb_addr==2 && end_count==2);
    assert(Inv_Archive_Delete(0)==-1 && g_inv_archive_lib.valid[0]);
    clear_error=0;
    assert(Inv_Archive_Delete(0)==0 && !g_inv_archive_lib.valid[0] && !g_inv_archive_proto[0]);
    int before=begin_count;
    assert(Inv_Archive_Delete(0)==0 && begin_count==before+1);
    assert(Inv_Archive_Delete(12)==-1 && begin_count==before+1);
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", default="gcc")
    args = parser.parse_args()
    store = function("cycle_loop/time_ctrl_init.c", "rt_err_t Time_Ctrl_Init_Clear_Archive(")
    runtime = function("cycle_loop/time_ctrl.c", "rt_err_t Time_Ctrl_Archive_Change_Begin(")
    runtime += function("cycle_loop/time_ctrl.c", "void Time_Ctrl_Archive_Change_End(")
    archive = function("inverter_manager/inverter_archive.c", "int8_t Inv_Archive_Set(")
    archive += function("inverter_manager/inverter_archive.c", "int8_t Inv_Archive_Delete(")
    queue = function("cycle_loop/inv_data.c", "void Inv_Control_Cancel_Queued(")
    with tempfile.TemporaryDirectory(prefix="time_ctrl_archive_") as directory:
        for name, code in (("store", STORE+store+STORE_CASES), ("runtime", RUNTIME+store+runtime+RUNTIME_CASES),
                           ("queue", QUEUE+queue+QUEUE_CASES), ("archive", ARCHIVE+archive+ARCHIVE_CASES)):
            source, binary = Path(directory)/f"{name}.c", Path(directory)/f"{name}.exe"
            source.write_text(COMMON+code, encoding="utf-8")
            subprocess.run([args.compiler, "-std=c99", "-Wno-unused-function", str(source), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)
            print(f"PASS: {name}")


if __name__ == "__main__":
    main()
