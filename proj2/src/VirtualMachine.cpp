//
// Created by failury on 4/24/20.
//

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include "VirtualMachine.h"
#include "Machine.h"
#include "VirtualMachine.h"
#include <iostream>
#include <vector>
#include <algorithm>
extern "C" {
#define PL() std::cout << "@ line" <<__LINE__
typedef void (*TVMMainEntry)(int, char*[]);
#define SMALL_BUFFER_SIZE       256
#define VMPrint(format, ...) VMFilePrint ( 1, format, ##__VA_ARGS__)
#define VMPrintError(format, ...) VMFilePrint ( 2, format, ##__VA_ARGS__)
#define VM_TIMEOUT_INFINITE ((TVMTick)0)
#define VM_TIMEOUT_IMMEDIATE ((TVMTick)-1)
void Scheduler(TVMThreadState State);
volatile int ticks = 0;
volatile int tickCount = 0;
TVMThreadID RunningThreadID;
struct TCB {
    //define tcb model
    TVMThreadID ID;
    TVMThreadState State;
    TVMThreadPriority Priority;
    TVMThreadEntry Entry;
    SMachineContext Context;
    int SleepTime;
    TVMMemorySize MemorySize;
    void *Param;
    void *StackAddress;
    int FileData;
};
std::vector<TCB> ThreadList;
std::vector<TCB> Ready;
std::vector<TCB> Sleep;
bool ComparePrio(const TCB &a, const TCB &b)
{
    return b.Priority <= a.Priority;
}
void Callback(void *CallData) {
    TMachineSignalState signal;
    MachineSuspendSignals(&signal);
    for (int i = 0; i < (int)Sleep.size(); i++) {
        Sleep[i].SleepTime--;
        if (Sleep[i].SleepTime < 1) {
            TCB AwakeThread = Sleep[i];
            Sleep.erase(Sleep.begin() + i);
            AwakeThread.State = VM_THREAD_STATE_READY;
            Ready.push_back(AwakeThread);
            Scheduler(VM_THREAD_STATE_READY);
        }
//        char tmp[12]={0x0};
//        sprintf(tmp,"%11d", int(Sleep[i].SleepTime));
//        write(STDOUT_FILENO,tmp,sizeof(tmp));
    }
    MachineResumeSignals(&signal);
}
void Scheduler(TVMThreadState State) {
    TMachineSignalState signal;
    MachineSuspendSignals(&signal);
    TCB NextThread;
    int TempID = RunningThreadID;
    NextThread = ThreadList[1];
    while (!Ready.empty()){
        std::sort(Ready.begin(),Ready.end(),ComparePrio);
        NextThread = Ready.front();
        Ready.erase(Ready.begin());
        if ( ThreadList[NextThread.ID].State == VM_THREAD_STATE_READY){
            NextThread = ThreadList[NextThread.ID];
        }
    }
    if(State == VM_THREAD_STATE_READY){
        ThreadList[RunningThreadID].State = VM_THREAD_STATE_READY;
        if(RunningThreadID != 1 ){
            Ready.push_back(ThreadList[RunningThreadID]);
        }
    } else if (State == VM_THREAD_STATE_WAITING){
        ThreadList[RunningThreadID].State = VM_THREAD_STATE_WAITING;
        Sleep.push_back( ThreadList[RunningThreadID]);
    } else if ( State == VM_THREAD_STATE_DEAD){
        ThreadList[RunningThreadID].State = VM_THREAD_STATE_DEAD;
    }
    ThreadList[NextThread.ID].State = VM_THREAD_STATE_RUNNING;
    RunningThreadID = NextThread.ID;
    char tmp[12]={0x0};
    sprintf(tmp,"%11d", int(TempID));
    write(STDOUT_FILENO,tmp,sizeof(tmp));
    write(STDOUT_FILENO,"Switchto",8);
    sprintf(tmp,"%11d", int(NextThread.ID));
    write(STDOUT_FILENO,tmp,sizeof(tmp));
    write(STDOUT_FILENO,"\n",1);
    MachineContextSwitch(&ThreadList[TempID].Context, &ThreadList[NextThread.ID].Context);
    MachineResumeSignals(&signal);
}
void IdleThread(void *param) {
    MachineEnableSignals();
    while (1) {

    }
}

void skeleton(void *param) {
    int tid = *((int *) param);
    MachineEnableSignals();
    ThreadList[tid].Entry(ThreadList[tid].Param);
    VMThreadTerminate(ThreadList[tid].ID);
}
TVMStatus VMStart(int tickms, int argc, char *argv[]) {
    MachineInitialize();
    MachineRequestAlarm(tickms * 1000, Callback, NULL);
    TVMMainEntry entry = VMLoadModule(argv[0]);
    if (entry != NULL) {
        TVMThreadID id;
        MachineEnableSignals();
        VMThreadCreate(skeleton, NULL, 0x100000, VM_THREAD_PRIORITY_NORMAL, &id);
        VMThreadCreate(IdleThread, NULL, 0x100000, 0, &id);
        VMThreadActivate(id);
        ThreadList[0].State = VM_THREAD_STATE_RUNNING;
    } else {
        MachineTerminate();
        return VM_STATUS_FAILURE;
    }
    entry(argc, argv);
    VMUnloadModule();
    MachineTerminate();
    return VM_STATUS_SUCCESS;
}


TVMStatus VMTickMS(int *tickmsref) {
    if (tickmsref != NULL) {
        TMachineSignalState signal;
        MachineSuspendSignals(&signal);
        *tickmsref = ticks;
        MachineResumeSignals(&signal);
        return VM_STATUS_SUCCESS;
    } else {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
}
TVMStatus VMTickCount(TVMTickRef tickref) {
    if (tickref != NULL) {
        TMachineSignalState signal;
        MachineSuspendSignals(&signal);
        *tickref = tickCount;
        MachineResumeSignals(&signal);
        return VM_STATUS_SUCCESS;
    } else {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
}
TVMStatus
VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid) {
    if (entry == NULL || tid == NULL) {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    } else {
        TMachineSignalState signal;
        MachineSuspendSignals(&signal);
        //initialize TCB
        TCB thread{};
        thread.ID = ThreadList.size();
        *tid = thread.ID;
        thread.Priority = prio;
        thread.Entry = entry;
        thread.Param = param;
        thread.MemorySize = memsize;
        thread.SleepTime = 0;
        thread.StackAddress = new uint8_t[memsize];
        thread.Context = SMachineContext();
        ThreadList.push_back(thread);
        MachineResumeSignals(&signal);
    }

    return VM_STATUS_SUCCESS;
}
TVMStatus VMThreadDelete(TVMThreadID thread) {
    if (ThreadList[thread].State == VM_THREAD_STATE_DEAD) {
        TMachineSignalState signal;
        MachineSuspendSignals(&signal);
        ThreadList.erase(ThreadList.begin() + thread);
        MachineResumeSignals(&signal);
        return VM_STATUS_SUCCESS;
    } else if (thread > ThreadList.size() || thread < 0) {
        return VM_STATUS_ERROR_INVALID_ID;
    }
    return VM_STATUS_ERROR_INVALID_STATE;
}

TVMStatus VMThreadActivate(TVMThreadID thread) {
    TMachineSignalState signal;
    MachineSuspendSignals(&signal);
    if (thread > ThreadList.size() || thread < 0) {
        return VM_STATUS_ERROR_INVALID_ID;
    } else if (ThreadList[thread].State != VM_THREAD_STATE_DEAD) {
        return VM_STATUS_ERROR_INVALID_STATE;
    }
    MachineContextCreate(&ThreadList[thread].Context, skeleton, (void*) (&(ThreadList[thread].ID)), ThreadList[thread].StackAddress,
                         ThreadList[thread].MemorySize);
//    char tmp[12]={0x0};
//    write(STDOUT_FILENO,"||||",4);
//    sprintf(tmp,"%11d", int(ThreadList[thread].Priority));
//    write(STDOUT_FILENO,tmp,sizeof(tmp));
//    write(STDOUT_FILENO,"||||",4);
//    write(STDOUT_FILENO,"\n",1);
    ThreadList[thread].State = VM_THREAD_STATE_READY;
    if (ThreadList[thread].Priority > 0) Ready.push_back(ThreadList[thread]);
    if (ThreadList[thread].Priority > ThreadList[RunningThreadID].Priority) {
        ThreadList[RunningThreadID].State = VM_THREAD_STATE_READY;
        Ready.push_back(ThreadList[RunningThreadID]);
        Scheduler(VM_THREAD_STATE_READY);
    }

    MachineResumeSignals(&signal);
    return VM_STATUS_SUCCESS;
}
TVMStatus VMThreadTerminate(TVMThreadID thread) {
    if (thread > ThreadList.size() || thread < 0) {
        return VM_STATUS_ERROR_INVALID_ID;
    } else if (ThreadList[thread].State == VM_THREAD_STATE_DEAD) {
        return VM_STATUS_ERROR_INVALID_STATE;
    }
    TMachineSignalState signal;
    MachineSuspendSignals(&signal);

    if(ThreadList[thread].State == VM_THREAD_STATE_RUNNING){
        if(Sleep.empty() && Ready.empty()){
            MachineResumeSignals(&signal);
            return VM_STATUS_SUCCESS;
        }
        Scheduler(VM_THREAD_STATE_DEAD);
    }else if ( ThreadList[thread].State == VM_THREAD_STATE_WAITING){
        for ( int i =0; i < Sleep.size(); i++){
            if(thread == ThreadList[Sleep[i].ID].ID){
                Sleep.erase(Sleep.begin()+  i);
                break;
            }
        }
    }
    ThreadList[thread].State = VM_THREAD_STATE_DEAD;
    MachineResumeSignals(&signal);
    return VM_STATUS_SUCCESS;
}
TVMStatus VMThreadID(TVMThreadIDRef threadref) {
    if (threadref == NULL) {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    TMachineSignalState signal;
    MachineSuspendSignals(&signal);
    //Schedule()
    *threadref = RunningThreadID;
    MachineResumeSignals(&signal);
    return VM_STATUS_SUCCESS;
}
TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef state) {

    if (thread > ThreadList.size() || thread < 0) {
        return VM_STATUS_ERROR_INVALID_ID;
    } else if (state == NULL) {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    } else {
        *state = ThreadList[thread].State;
        return VM_STATUS_SUCCESS;
    }
}
TVMStatus VMThreadSleep(TVMTick tick) {
    if (tick == VM_TIMEOUT_INFINITE) {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    TMachineSignalState signal;
    MachineSuspendSignals(&signal);
    TCB& CurrentThread = ThreadList[RunningThreadID];
    CurrentThread.SleepTime = tick;
    CurrentThread.State = VM_THREAD_STATE_WAITING;
    Sleep.push_back(CurrentThread);
    Scheduler(VM_THREAD_STATE_WAITING);
    return VM_STATUS_SUCCESS;

}
void FileCallback(void *calldata, int result)
{
    TMachineSignalState signal;
    MachineSuspendSignals(&signal);
    TCB* thread = (TCB*)calldata;
    thread->FileData = result;
    thread->State = VM_THREAD_STATE_READY;
    Scheduler(VM_THREAD_STATE_READY);
    MachineResumeSignals(&signal);
}
TVMStatus VMFileOpen(const char *filename, int flags, int mode,
                     int *filedescriptor) {
    if (filename == NULL || filedescriptor == NULL) {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    TMachineSignalState signal;
    MachineSuspendSignals(&signal);
    MachineFileOpen(filename, flags, mode, FileCallback, &ThreadList[RunningThreadID]);
    ThreadList[RunningThreadID].State = VM_THREAD_STATE_WAITING;
    Scheduler(VM_THREAD_STATE_WAITING);
    *filedescriptor = ThreadList[RunningThreadID].FileData;
    MachineResumeSignals(&signal);
    return VM_STATUS_SUCCESS;
}
TVMStatus VMFileClose(int filedescriptor)
{
    TMachineSignalState signal;
    MachineSuspendSignals(&signal);
    MachineFileClose(filedescriptor, FileCallback, &ThreadList[RunningThreadID]);
    Scheduler(VM_THREAD_STATE_WAITING);
    MachineResumeSignals(&signal);
    return VM_STATUS_SUCCESS;
}
TVMStatus VMFileRead(int filedescriptor, void *data, int *length)
{
    TMachineSignalState signal;
    MachineSuspendSignals(&signal);
    if (length == NULL || data == NULL) {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    MachineFileRead(filedescriptor, data, *length, FileCallback, &ThreadList[RunningThreadID]);
    ThreadList[RunningThreadID].State = VM_THREAD_STATE_WAITING;
    Scheduler(VM_THREAD_STATE_WAITING);
    *length = ThreadList[RunningThreadID].FileData;
    MachineResumeSignals(&signal);
    return VM_STATUS_SUCCESS;
}
TVMStatus VMFileWrite(int filedescriptor, void *data, int *length)
{
    TMachineSignalState signal;
    MachineSuspendSignals(&signal);
    if (length == NULL || data == NULL) {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    MachineFileWrite(filedescriptor, data, *length, FileCallback, &ThreadList[RunningThreadID]);
    ThreadList[RunningThreadID].State = VM_THREAD_STATE_WAITING;
    Scheduler(VM_THREAD_STATE_WAITING);
    MachineResumeSignals(&signal);
    return VM_STATUS_SUCCESS;
}
TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, int *newoffset)
{
    TMachineSignalState signal;
    MachineSuspendSignals(&signal);
    if (newoffset == NULL) {
        return VM_STATUS_FAILURE;
    }
    *newoffset = offset;
    MachineFileSeek(filedescriptor, offset, whence, FileCallback, &ThreadList[RunningThreadID]);
    ThreadList[RunningThreadID].State = VM_THREAD_STATE_WAITING;
    Scheduler(VM_THREAD_STATE_WAITING);
    MachineResumeSignals(&signal);
    return VM_STATUS_SUCCESS;

}
}
