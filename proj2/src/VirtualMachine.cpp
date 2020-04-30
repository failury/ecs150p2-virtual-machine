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

extern "C" {
#define PL() std::cout << "@ line" <<__LINE__
typedef void (*TVMMainEntry)(int, char *[]);
void Scheduler( TVMThreadState State);
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
    TVMTick SleepTime;
    TVMMemorySize MemorySize;
    void *Param;
    void *StackAddress;
};
std::vector<TCB> ThreadList;
std::vector<TCB> Ready;
std::vector<TCB> Sleep;
void Callback(void* CallData){
    TMachineSignalState signal;
    MachineSuspendSignals(&signal);
    for(int i = 0; i < Sleep.size(); i++)
    {
        Sleep[i].SleepTime--;
        if (Sleep[i].SleepTime < 1 ){
            TCB AwakeThread = Sleep[i];
            Sleep.erase(Sleep.begin() + i);
            AwakeThread.State = VM_THREAD_STATE_READY;
            Ready.push_back(AwakeThread);
            Scheduler(VM_THREAD_STATE_READY);
        }
    }
    MachineResumeSignals(&signal);
}
void Scheduler( TVMThreadState State){
    TCB NextThread;
    for (int i = 0; i < Ready.size(); i++){
        if (Ready[i].Priority == VM_THREAD_PRIORITY_HIGH){
            NextThread = Ready[i];
            Ready.erase(Ready.begin() + i);
            i = 0;
            continue;
        } else if (Ready[i].Priority == VM_THREAD_PRIORITY_NORMAL){
            NextThread = Ready[i];
            Ready.erase(Ready.begin() + i);
            i = 0;
            continue;
        } else if (Ready[i].Priority == VM_THREAD_PRIORITY_LOW){
            NextThread = Ready[i];
            Ready.erase(Ready.begin() + i);
            i = 0;
            continue;
        }
    }
    if (State == VM_THREAD_STATE_WAITING){
        Sleep.push_back(ThreadList[RunningThreadID]);
    }
    ThreadList[NextThread.ID].State = VM_THREAD_STATE_RUNNING;
    TVMThreadID TempID = RunningThreadID;
    RunningThreadID = NextThread.ID;
    MachineContextSwitch(&ThreadList[TempID].Context,&ThreadList[NextThread.ID].Context);
}
void IdleThread(void* param){
    while (1){

    }
}
TVMStatus VMStart(int tickms, int argc, char *argv[]) {
    MachineInitialize();
    MachineRequestAlarm(tickms*1000, Callback, NULL);
    TVMMainEntry entry = VMLoadModule(argv[0]);
    if (entry != NULL) {
        TVMThreadID id;
        VMThreadCreate(IdleThread,NULL,0x10000,VM_THREAD_PRIORITY_NORMAL,&id);
        VMThreadActivate(id);
        MachineEnableSignals();
        entry(argc, argv);
    } else {
        MachineTerminate();
        return VM_STATUS_FAILURE;
    }

    Scheduler(VM_THREAD_STATE_WAITING);
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
TVMStatus VMFileWrite(int filedescriptor, void *data, int *length) {
    if (data == NULL) {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    } else {
        write(STDOUT_FILENO, data, *length);
        return VM_STATUS_SUCCESS;
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
void skeleton(void *param) {
    MachineEnableSignals();
    int tid = *((int *) param);
    ThreadList[tid].Entry(ThreadList[tid].Param);
    VMThreadTerminate(ThreadList[tid].ID);
}
TVMStatus VMThreadActivate(TVMThreadID thread) {
    if (thread > ThreadList.size() || thread < 0) {
        return VM_STATUS_ERROR_INVALID_ID;
    } else if (ThreadList[thread].State != VM_THREAD_STATE_DEAD) {
        return VM_STATUS_ERROR_INVALID_STATE;
    }
    TMachineSignalState signal;
    MachineSuspendSignals(&signal);
    TCB CurrentThread = ThreadList[thread];
    MachineContextCreate(&CurrentThread.Context, skeleton, NULL, CurrentThread.StackAddress,
                         CurrentThread.MemorySize);
    CurrentThread.State = VM_THREAD_STATE_READY;
    Ready.push_back(CurrentThread);
    if (CurrentThread.Priority > ThreadList[RunningThreadID].Priority) {
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
    //Schedule()
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
    TCB CurrentThread = ThreadList[RunningThreadID];
    CurrentThread.SleepTime = tick;
    CurrentThread.State = VM_THREAD_STATE_WAITING;
    Sleep.push_back(CurrentThread);
    MachineEnableSignals();
    Scheduler(VM_THREAD_STATE_WAITING);
    return VM_STATUS_SUCCESS;

}

//TVMStatus VMFilePrint(int filedescriptor, const char *format, ...) {
//
//}
//
//TVMStatus VMFileOpen(const char *filename, int flags, int mode,
//                     int *filedescriptor) {
//    if (filename == NULL || filedescriptor == NULL) {
//        return VM_STATUS_ERROR_INVALID_PARAMETER;
//    }
//    TMachineSignalState signal;
//    MachineSuspendSignals(&signal);
//    MachineFileOpen(filename, flags, mode, FileCallback, &fs);
//}
}
