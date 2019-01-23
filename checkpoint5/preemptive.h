/*
 * file: cooperative.h
 *
 * this is the include file for the cooperative multithreading
 * package.  It is to be compiled by SDCC and targets the EdSim51 as
 * the target architecture.                                       */


#ifndef __PREEMPTIVE_H__
#define __PREEMPTIVE_H__

#define MAXTHREADS 4  /* not including the scheduler */
/* the scheduler does not take up a thread of its own */

typedef char ThreadID;
typedef void (*FunctionPtr)(void);

ThreadID ThreadCreate(FunctionPtr);
void ThreadYield(void);
void ThreadExit(void);


/*************************************************************************************************
 * Add a semaphore API to preemptive.h and preempt.c if necessary, 
 * depending on whether you implement it as a macro or as a function.  
 * A macro simply defines the inline assembly code and gives you precise control 
 * over the implementation; so we will assume a macro implementation, 
 * although you may also choose code implementation.
 *
 * The basic semaphore API to implement consists of
 *
 * #define SemaphoreCreate(s, n)   // create a counting semaphore s initialized to n
 * #define SemaphoreWait(s)          // do busy-wait on semaphore s
 * #define SemaphoreSignal(s)       // signal a semaphore s
 *
 * ================================================================================================
 * [1.1 SemaphoreCreate(s, n)]
 * SemaphoreCreate simply initializes the semaphore s (an “integer”, but we will just use a char) 
 * to the value n.  In the """busy-waiting""" version, that is all it takes.  
 * You can write this as a macro in C if you want, since it is just a simple assignment.   
 * You can also write it as inlined assembly if you want, but you can do that optimization later.
 * 
 * [Extra credit]  If you are doing the """non-busy-waiting""" version, then you will want 
 * to initialize any associated data structures, like the list of threads that are blocked 
 * on this semaphore.
 *
 * ================================================================================================
 * [1.2  SemaphoreSignal(s)]
 * SemaphoreSignal() is very simple for the busy-waiting version of semaphore: simply increment 
 * the semaphore variable.  It is recommended that you write this in assembly, because you can 
 * do this as an atomic operation (i.e., single assembly instruction), namely INC.  
 * The operand itself can refer to the symbol by prepending it with an underscore (_).
 *
 * However, if you define it as a macro argument, e.g., #define SemaphoreSignal(s), if you write _s, 
 * the s part will not get expanded into the actual name of the semaphore!  Fortunately, C macro 
 * preprocessor includes the ## operator, which lets you concatenate two symbols together. 
 * For convenience, you can define another macro for converting a C name into an assembly name:
 * 		#define CNAME(s) _ ## s
 * This way, if you write CNAME(mutex), it gets macro-expanded into _mutex, which is exactly 
 * what you want.
 *
 * [Extra Credit] The non-busy-waiting version actually does more: it also needs to go through 
 * the waiting list and pick one process to wake up.
 *
 * ================================================================================================
 * 1.3  SemaphoreWait(s)
 * This one is a bit more work, but still not too difficult.  The pseudocode looks like
 *     Wait(S) :
 *         while (S <= 0) { } // busy wait
 *         S--;
 * One thing to notice is that since the value of S is expected to drop below zero, 
 * do not declare the semaphore as an unsigned!
 * You could write this in C, but SDCC might not compile it to the right code.  Look at the .asm file
 * to see how it compiles the code.  If you can get it working properly in C, then that is fine. 
 * However, it is recommended that you write assembly code for this.
 *
 * The assembly code looks something like this:
 * #define SemaphoreWaitBody(S, label) \
 *     { __asm \
 *       label: ;; top of while-loop \
 *              ;; read value of _S into accumulator (where S is the semaphore) \
 *              ;; use conditional jump(s) to jump back to label if accumulator <= 0  \
 *              ;; fall-through to drop out of while-loop \
 *       dec  CNAME(S) \
 *       __endasm; }
 *
 * The list of conditional branch instructions in 8051 are
 * JZ  label        ;; jump if accumulator is zero
 * JNZ label        ;; jump if accumulator is not zero
 * JB bit, label    ;; jump if bit is 1
 * JNB bit, label   ;; jump if bit is 0 
 *
 * Hint: if ACC.7 bit (the sign bit) is 1, then the number in the accumulator is negative.
 *
 * However, there is one issue with the assembly label.  Because we are defining it as a macro, 
 * and we use multiple semaphores in the same code, we need to give each instance a different label,
 * so the instruction can jump to the proper label.  However, we don’t want the caller to have to 
 * come up with a unique label each time they call SemaphoreWait.  There is one simple solution: 
 * C preprocessor defines the __COUNTER__ name, which will generate a new integer each time it is 
 * used.  In SDCC, user labels in the form of 1$, 2$, … up to 100 can be used without conflict with 
 * the assembler’s labels.  So we can define SemaphoreWait to pass a unique label based on 
 * __COUNTER__ to SemaphoreWaitBody each time.  Just be sure you concatenate __COUNTER__ with 
 * the $ to form the label.
 *
 * [Extra Credit]  The non-busy-waiting version will need to put the thread to a list of waiting 
 * processes, instead of busy waiting.  But if the thread does not have to wait then it just does 
 * the decrement.  Note that at the time of getting waken, the semaphore value can still be negative
 * So this means you don’t keep testing the while (s <= 0); when you are waken, you just decrement 
 * the s anyway, even if s is still negative!
 *
 *
 * Date: 24/12/2018
 ***************************************************************************************************/

#define BUSY_WAITING 1
#ifdef BUSY_WAITING

///////Busy-waiting version////////
#define CNAME(s) _##s
#define SemaphoreCreate(s, n) s = n

#define SemaphoreSignal(s) SEMAPHORE_SIGNAL(CNAME(s))
#define SEMAPHORE_SIGNAL(var_sem) \
    __asm \
        inc var_sem \
    __endasm

/*  Note: The MACRO problem takes me one day to figure out.
 *  Ref: https://cboard.cprogramming.com/c-programming/151672-llvm-__counter__-macro-not-expanding.html
 *       https://stackoverflow.com/questions/19666142/why-is-a-level-of-indirection-needed-for-this-concatenation-macro
 *  Date: 26/12/2018  																				           ********/

#define CONCAT( x, y ) x##y
#define SemaphoreWait(s) _SemaphoreWait(_##s, __COUNTER__)
#define _SemaphoreWait(var_sem, lable1) _SemaphoreWait_(var_sem, lable1)
#define _SemaphoreWait_(var_sem, lable2) SemaphoreWaitBody(var_sem, lable2##$)
#define SemaphoreWaitBody(S, lable) \
    { __asm \
        lable: MOV ACC, S \
               NOP \
               NOP \
               NOP \
               JZ lable \
               JB ACC.7, lable \
               DEC S \
      __endasm; }

#else

///////Non-Busy-waiting version/////
//Pending
#define SemaphoreCreate(s,n) s = n
#define SemaphoreSignal(s)
#define SemaphoreWait(s)


#endif // BUSY_WAITING
#endif //__PREEMPTIVE_H__
