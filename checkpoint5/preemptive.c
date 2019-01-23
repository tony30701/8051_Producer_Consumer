#include <8051.h>
#include <stdio.h>
#include <string.h>
#include "preemptive.h"

/*
 * @@@ [2 pts] declare the static globals here using 
 *        __idata __at (address) type name; syntax
 * manually allocate the addresses of these variables, for
 * - saved stack pointers (MAXTHREADS)
 * - current thread ID
 * - a bitmap for which thread ID is a valid thread; 
 *   maybe also a count, but strictly speaking not necessary
 * - plus any temporaries that you need.
 */

__data __at (0x20) char savedSP[MAXTHREADS];
__data __at (0x24) ThreadID threadID;
__data __at (0x25) ThreadID currentThread = MAXTHREADS;
__data __at (0x26) char bitmap = 0x0;
__data __at (0x27) char threadCount; //threadCount使用__data & char
__data __at (0x28) char shift = 0x0;
__data __at (0x29) char mask = 0x1;
__data __at (0x36) char SP_temp;
__data __at (0x5F) char PSW_temp;

/* we declare main() as an extern so we can reference its symbol when creating a thread for it. */
extern void main(void);
extern char thread_empty;  //__at (0x34)
extern unsigned char counterISR; //__at (0x5E)
extern char rear;
/**********************************************************************************
 * 
 * Check to make sure your assembly code saves registers before trashing them, 
 * as explained in the lecture on 11/7.   
 * It can be the same as the cooperative version.
 *
 **********************************************************************************/

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



/****************************************************************************************************
 *
 * In Bootstrap, you can set up Timer 0 to cause preemption.  (Timer 1 is already used by UART).  
 * If you want to use Timer 0 in mode 0, you can add this code
 * TMOD = 0;   // timer 0 mode 0
 * IE = 0x82;  // enable timer 0 interrupt; keep consumer polling
 *             // EA  -  ET2  ES  ET1  EX1  ET0  EX0
 * TR0 = 1; // set bit TR0 to start running timer 0
 * before you create the initial thread and context switch to it.
 *
 * Date: 21/12/2018
 ******************************************************************************************************/

void Bootstrap(void) {
    /*
    * @@@ [2 pts]
    * initialize data structures for threads (e.g., mask)
    * optional: move the stack pointer to some known location
    * only during bootstrapping. by default, SP is 0x07.
    *
    * @@@ [2 pts]
    *     create a thread for main; be sure current thread is
    *     set to this thread ID, and restore its context,
    *     so that it starts running main().
    */
    
    //Initilize global variable
    counterISR = 0;
    rear = 0;


    // Turn on Timer 0
    TMOD = 0x20;
    IE = 0x82;  //EA - - ES ET1 EX1 ET0 EX0
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


/*************************************************************************************************
 * ThreadCreate(), ThreadYield(), ThreadExit()
 * These functions are largely the same as the cooperative version, 
 * except we want to make sure it is executed atomically.  
 * To do this, you can use the __critical tag just before the { } for the entire function’s body; 
 * or you may set and clear EA bit before and after the function body code.
 *
 * Date: 21/12/2018
 *
 * ThreadCreate() creates a thread data structure so it is ready
 * to be restored (context switched in).
 * The function pointer itself should take no argument and should
 * return no argument.
 *
 **************************************************************************************************/
ThreadID ThreadCreate(FunctionPtr fp) {

    
    //儲存當前的 Stack Pointer, 以便於後續將 SP 指向 new thread 的 Stack Space,
    //在將 new thread 的資訊放入 Stack 之後, 再將 SP 指回原來的 Stack Space     
    SP_temp = SP;
    PSW_temp = PSW;         
    shift = 0;
    mask = 0x01;

    /* **************************************************
     * Check to see we have not reached the max #threads.
     * if so, return -1, which is not a valid thread ID.
     *
     * **************************************************/
    if (threadCount + 1 > (char) MAXTHREADS){  
        return -1; 
    }
    threadCount = threadCount + 1;


    /* ***************************************************
     * Otherwise, find a thread ID that is not in use,
     * and grab it. (can check the bit mask for threads),
     *
     * ***************************************************/

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
  
 
	/******************** 
	 * d. push the return address fp (2-byte parameter to
	 *    ThreadCreate) onto stack so it can be the return
	 *    address to resume the thread. Note that in SDCC
	 *    convention, 2-byte ptr is passed in DPTR.  but
	 *    push instruction can only push it as two separate
	 *    registers, DPL and DPH
	 * 
     * Ref: https://sourceforge.net/p/sdcc/mailman/sdcc-user/?viewmonth=201003
     *
	 ************                                              ************/
    DPL = (unsigned int)fp & 0xff;
    DPH = (unsigned int)fp >> 8;
    __asm
        PUSH DPL
        PUSH DPH
    __endasm;


    /****** temporary section ******/
    __asm
        PUSH ar7
        PUSH ar6
        PUSH ar5
    __endasm;

	/* ***********************************************
	 * e. we want to initialize the registers to 0, so we
	 *	  assign a register to 0 and push it four times
	 *	  for ACC, B, DPL, DPH.  Note: push #0 will not work
	 *	  because push takes only direct address as its operand,
	 *	  but it does not take an immediate (literal) operand.
	 * ******                                         *********/       
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

	/* *************************************
	 * f. finally, we need to push PSW (processor status word)
	 *    register, which consist of bits
	 *     CY AC F0 RS1 RS0 OV UD P
	 *    all bits can be initialized to zero, except <RS1:RS0>
	 *    which selects the register bank.
	 *    Thread 0 uses bank 0, Thread 1 uses bank 1, etc.
	 *    Setting the bits to 00B, 01B, 10B, 11B will select
	 *    the register bank so no need to push/pop registers
	 *    R0-R7.  So, set PSW to
	 *    00000000B for thread 0, 00001000B for thread 1,
	 *    00010000B for thread 2, 00011000B for thread 3.
	 * *****                                 *****************/
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

   
	/* **************************
	 * g. write the current stack pointer to the saved stack
	 *    pointer array for this newly created thread ID
	 * h. set SP to the saved SP in step c.
	 * i. finally, return the newly created thread ID.
	 * *******                                   ***********/
    savedSP[threadID] = SP;
    SP = SP_temp;
    PSW = PSW_temp;
    return (threadCount-1);
}



/* **************
 * this is called by a running thread to yield control to another
 * thread.  ThreadYield() saves the context of the current
 * running thread, picks another thread (and set the current thread
 * ID to it), if any, and then restores its state.
 * *******                                         ****************/
void ThreadYield(void) {

    // (1) SAVESTATE
    // (2) pick next thread 
    // (3) RESTORESTATE 
    SAVESTATE;
    do {

        /* ************************************
         * @@@ [8 pts] do round-robin policy for now.
         * find the next thread that can run and
         * set the current thread ID to it,
         * so that it can be restored (by the last line of
         * this function).
         * there should be at least one thread, so this loop
         * will always terminate.
         * *******                               ***********/

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


/* ***********
 * ThreadExit() is called by the thread's own code to termiate
 * itself.  It will never return; instead, it switches context
 * to another thread.
 * *****                                              *******/
void ThreadExit(void) {

    /* ***************
     * clear the bit for the current thread from the
     * bit mask, decrement thread count (if any),
     * and set current thread to another valid ID.
     * Q: What happens if there are no more valid threads?
     * ***********                                *******/
    //RESTORESTATE;

    shift = currentThread;
    mask = 0x01;
    mask = mask << shift;

    //[1]清除 bitmap 內對應的 bit 以及將 threadCount 減1
    threadCount = threadCount - 1;
    bitmap = bitmap ^ mask;

    //[2]釋放 thread 的名額給其它 thread
    SemaphoreSignal(thread_empty);

    //[3]return 回去原本的Car thread
    __asm
        ret
    __endasm;
}


/***************************************************************************************************************************
 * myTimer0Handler()
 *
 * This is the new routine you need to write to be the ISR for Timer0, which serves the purpose of preemption.  
 * A straightforward implementation would be to copy the code for ThreadYield() but instead of relying on the compiler 
 * to generate RET instruction to return to a function (or subroutine) call, you need to put in the RETI assembly instruction 
 * to return from the interrupt (after all, it is invoked as an interrupt service routine).
 *
 * However, depending on how you write the ThreadYield(), if you write parts of it in C between SAVESTATE and RESTORESTATE, 
 * it is likely to use registers (especially R0 and R1 if it needs to use pointers to IDATA).  
 * Because it only saves the bank numbers but not copying the register values, any code that modifies R0 - R7 will trash them 
 * and their values cannot be restored by RESTORESTATE.
 *
 * One solution is for you to insert code to preserve the value of any such registers by copying them to registers 
 * that have been saved (e.g., B, DPH, DPL, etc., or your designated memory locations) after SAVESTATE, 
 * and copy them back to those registers before the RESTORESTATE.   This is the quickest way to get working code.
 *
 * Another solution, which may be more robust, is to reserve one thread just for the thread manager, 
 * so that you always switch context to this thread (which runs outside the ISR), and it has its own register set and stack, 
 * so that trashing R0-R7 is not an issue.  It also has the advantage of keeping the ISR short, especially if the scheduler itself 
 * gets more complex. However, it also means using one thread, so the user has now one fewer thread.
 *
 * Date: 21/12/2018
 ****************************************************************************************************************************/

void myTimer0Handler(void){

    counterISR++;
 
    SAVESTATE;
    do{
        /**************************************************************************
         * round-robin policy
         * find the next thread that can run
         * set the current thread ID
         * so that it can be restored (by the last line of the function)
         * there should be at least one thread, so this loop will always terminate.
         * 
         * Date: 21/12/2018
         ***************************************************************************/
        
        // currentThread 只會是 0, 1, 2, 3;  
        // 因此若 currentThread == MAXTHREADS == 4, 就表示沒有任何thread 被建立
        // 此時只能選擇 reti, 因為上一個步驟的 SAVESTATE 的 Push register 的 Stack 是不存在的是存在 saved[MAXTHREADS], 這是個錯誤的位置
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


        /********************
        //Version2 : Modified Round-Robin for thread switch
        //若4個thread還沒全部被建立起來, 則持續執行 thread 0, 
        //否則就是 thread 0 -> thread 1 -> thread 2 -> thread 3 的切換順序。
        if (currentThread == 0){
            if ( (bitmap >> (currentThread + 1)) & 1 && (bitmap >> (currentThread + 2)) & 1 && (bitmap >> (currentThread + 3)) & 1 )
                currentThread = 1;
            else
                currentThread = 0;
        }else if (currentThread == 1){
            if ( (bitmap >> (currentThread + 1)) & 1)
                currentThread = 2;
            else
                currentThread = 0;
        }else if (currentThread == 2){
            if ( (bitmap >> (currentThread + 1)) & 1)
                currentThread = 3;
            else
                currentThread = 0;
        }else{
            currentThread = 0;
        }*********************                                  **********/   
                   									    		 	
        break;
    }while(1);                                    

    RESTORESTATE;

    __asm
        reti
    __endasm;
}
