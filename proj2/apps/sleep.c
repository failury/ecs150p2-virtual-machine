#include "VirtualMachine.h" 	 	    		

void VMMain(int argc, char *argv[]){
    VMPrint("Going to sleep for 30 ticks\n");
    VMThreadSleep(30);
    VMPrint("Awake\nGoodbye\n");
}

