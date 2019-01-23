#include <8051.h>
#include <stdio.h>
#include "preemptive.h"

__data __at (0x20) char savedSP[MAXTHREADS];
__data __at (0x24) ThreadID threadID;
__data __at (0x25) ThreadID currentThread = MAXTHREADS;
__data __at (0x26) char bitmap;
__data __at (0x27) char threadCount;
__data __at (0x28) char shift = 0x0;
__data __at (0x29) char mask = 0x1;
__data __at (0x2A) char SP_temp;
__data __at (0x5F) char PSW_temp;

/* we declare main() as an extern so we can reference its symbol when creating a thread for it. */
extern void main(void);
extern char thread_empty;  //__at (0x34)
extern unsigned char counterISR; //__at (0x5E)
extern char rear;

#define SAVESTATE \
    { __asm \
          push ar7 \
          push ar6 \
          push ar5 \
          push A \
          push B \
          push DPL \
          push DPH \
          push PSW \
      __endasm; \
      savedSP[currentThread] = SP; \
    }

#define RESTORESTATE \
    { SP = savedSP[currentThread]; \
       __asm \
           pop PSW \
           pop DPH \
           pop DPL \
           pop B \
           pop A \
           pop ar5 \
           pop ar6 \
           pop ar7 \
       __endasm; \
     }

void Bootstrap(void) {

    // Turn on Timer 0
    TMOD = 0x20;
    IE = 0x82;
    TR0 = 1;

    // (1) initialize thread mgr vars 
    bitmap = 0x00;
    threadCount = 0;
    SP = 0x07;
 
    // (2) create thread for main
    // (3) set current thread ID
    threadID = ThreadCreate(main);
    currentThread = threadID;

    // (4) restore
    RESTORESTATE; 
    __asm
        ret
    __endasm;
}

ThreadID ThreadCreate(FunctionPtr fp) {
    
    char count = 0;
    char i = 0;

    // 儲存當前的 Stack Pointer, 以便於後續將 SP 指向 new thread 的 Stack Space,
    // 在將 new thread 的資訊放入 Stack 之後, 再將 SP 指回原來的 Stack Space
    SP_temp = SP;
    PSW_temp = PSW;
    shift = 0;
    mask = 0x01;

    // Check to see we have not reached the max #threads.
    // if so, return -1, which is not a valid thread ID.
    if (threadCount + 1 > (char) MAXTHREADS){
        return -1;
    }
    threadCount = threadCount + 1;


    // Otherwise, find a thread ID that is not in use,
    // and grab it. (can check the bit mask for threads),

    // a. update the bit mask (and increment thread count, if you use a thread count, but it is optional)
    // 可以進得來ThreadCreate(FunctionPtr fp) 就表示有空的 thread 可以使用, 因此shift頂多等於3, 不會超過3
    while ( (shift <= 3) && ((bitmap >> shift) & 0x1) ){
        shift = shift + 1;
    }
    mask = mask << shift;
    bitmap = bitmap | mask;
    threadID = shift;

    // b. calculate the starting stack location for new thread
    // c. save the current SP in a temporary set SP to the starting location for the new thread

    if (threadID == 0){
        SP = 0x3F;
    }else if (threadID == 1){
        SP = 0X4F;
    }else if (threadID == 2){
        SP = 0x5F;
    }else  // threadID == 3
        SP = 0x6F;

   
	
    // d. push the return address fp (2-byte parameter to ThreadCreate) onto stack so it can be the return
	//    address to resume the thread. Note that in SDCC convention, 2-byte ptr is passed in DPTR.  but
	//    push instruction can only push it as two separate registers, DPL and DPH
	// 
    // Ref: https://sourceforge.net/p/sdcc/mailman/sdcc-user/?viewmonth=201003

    DPH = (unsigned int)fp >> 8;
    DPL = (unsigned int)fp & 0xff;
    __asm
        PUSH DPL
        PUSH DPH
    __endasm;

    __asm
        PUSH ar7
        PUSH ar6
        PUSH ar5
    __endasm;


    // e. we want to initialize the registers to 0, so we assign a register to 0 and push it four times
	//	  for ACC, B, DPL, DPH.  Note: push #0 will not work because push takes only direct address as its operand,
	//	  but it does not take an immediate (literal) operand.       

    __asm 
        MOV R7, #0
        MOV ACC, R7
        MOV B, R7
        MOV DPL, R7
        MOV DPH, R7
        PUSH ACC
        PUSH B
        PUSH DPL
        PUSH DPH
    __endasm;


	/* **********************
	 * f. finally, we need to push PSW (processor status word) register, which consist of bits
	 *       CY AC F0 RS1 RS0 OV UD P
	 *    all bits can be initialized to zero, except <RS1:RS0> which selects the register bank.
	 *    Thread 0 uses bank 0, Thread 1 uses bank 1, etc. Setting the bits to 00B, 01B, 10B, 11B will select
	 *    the register bank so no need to push/pop registers R0-R7.  So, set PSW to
	 *    00000000B for thread 0, 00001000B for thread 1,
	 *    00010000B for thread 2, 00011000B for thread 3.                                    *****************/

    if (threadID == 0){
        PSW = 0x00;
    }else if (threadID == 1){
        PSW = 0x08;
    }else if (threadID == 2){
        PSW = 0x10;
    }else  // threadID == 3
        PSW = 0x18;
    __asm
        PUSH PSW
    __endasm;


	// g. write the current stack pointer to the saved stack pointer array for this newly created thread ID
    // h. set SP to the saved SP in step c.
	// i. finally, return the newly created thread ID.

    savedSP[threadID] = SP;
    SP = SP_temp;
    PSW = PSW_temp;
    return (threadCount-1);
}

void ThreadYield_extra(char tid) {

    SAVESTATE;
    do {
        currentThread = tid;
        break;
    } while(1);
    RESTORESTATE;
    __asm
        ret
    __endasm;
}

void ThreadYield(void) {

    SAVESTATE;
    do {
        shift = currentThread;
        do {
            shift = (shift + 1) % (char)MAXTHREADS;
            if ((bitmap >> shift) & 0x1){
                currentThread = shift;
                break;
            }
        } while(1);

        break;
    } while(1);
    RESTORESTATE;
    __asm
        ret
    __endasm;
}

void ThreadExit(void) {

    shift = currentThread;
    mask = 0x01;
    mask = mask << shift;

    //[1]清除 bitmap 內對應的 bit 以及將 threadCount 減1
    threadCount = threadCount - 1;
    bitmap = bitmap ^ mask;

    //[2]釋放 thread 的名額給其它 thread
    //SemaphoreSignal(thread_empty);

    //[3]return 回去原本的Car thread
    __asm
        ret
    __endasm;
}

void myTimer0Handler(void){

    counterISR++;

    SAVESTATE;
    do{

        // currentThread 只會是 0, 1, 2, 3;
        // 因此若 currentThread == MAXTHREADS == 4, 就表示沒有任何thread 被建立
        // 此時只能選擇 reti, 因為上一個步驟的 SAVESTATE 的 Push register 的 Stack 是不存在的, 
        // 沒有分配第5個位置 saved[4], 這是個錯誤的位置
        if (currentThread == MAXTHREADS){
            __asm
                reti
            __endasm;
        }

        //Version1 : Default Round-Robin for thread switch
        shift = currentThread;
        do{
            shift = (shift + 1) % (char)MAXTHREADS;
            if ((bitmap >> shift) & 0x1){
                currentThread = shift;
                break;
            }
        }while(1);

        break;
    }while(1);

    RESTORESTATE;
    __asm
        reti
    __endasm;
}

