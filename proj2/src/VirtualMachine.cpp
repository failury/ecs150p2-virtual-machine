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

extern "C"{
typedef void (*TVMMainEntry) (int, char*[]);
volatile int ticks = 0;
TVMStatus VMStart(int tickms, int argc, char *argv[]){
    TVMMainEntry entry = VMLoadModule(argv[0]);
    if(entry != NULL){
        MachineInitialize();
        MachineEnableSignals();
        entry(argc, argv);

    }else {
        MachineTerminate();
        return VM_STATUS_FAILURE;
    }
    VMUnloadModule();
    MachineTerminate();
    return VM_STATUS_SUCCESS;
}


TVMStatus VMTickMS(int *tickmsref){
    if(tickmsref != NULL){
        *tickmsref = ticks;
        return VM_STATUS_SUCCESS;
    }else {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
}
TVMStatus VMFileWrite(int filedescriptor, void *data, int *length){
    if (data == NULL){
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    else{
        write(STDOUT_FILENO,data,*length);
        return VM_STATUS_SUCCESS;
    }
}
}
