
// file: preemptive.h
/********************************************** ******/

#ifndef __PREEMPTIVE_H__
#define __PREEMPTIVE_H__

#define MAXTHREADS 4  /* not including the scheduler */
typedef char ThreadID;
typedef void (*FunctionPtr)(void);

ThreadID ThreadCreate(FunctionPtr);
void ThreadYield(void);
void ThreadYield_extra(char);
void ThreadExit(void);

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


#define BUSY_WAITING 1
#ifdef BUSY_WAITING

///////////////////////////////////////////////////////////////////////// Busy-waiting version ////////
#define CNAME(s) _##s
#define SemaphoreCreate(s, n) s = n

#define SemaphoreSignal(s) SEMAPHORE_SIGNAL(CNAME(s))
#define SEMAPHORE_SIGNAL(var_sem) \
    __asm \
        inc var_sem \
    __endasm

/* ************************
 * Note: The MACRO problem takes me one day to figure out.
 * Ref: https://cboard.cprogramming.com/c-programming/151672-llvm-__counter__-macro-not-expanding.html
 *      https://stackoverflow.com/questions/19666142/why-is-a-level-of-indirection-needed-for-this-concatenation-macro
 * Date: 26/12/2018  																************************ ********/

#define CONCAT( x, y ) x##y
#define SemaphoreWait(s) _SemaphoreWait(_##s, __COUNTER__)
#define _SemaphoreWait(var_sem, lable1) _SemaphoreWait_(var_sem, lable1)
#define _SemaphoreWait_(var_sem, lable2) SemaphoreWaitBody(var_sem, lable2##$)
#define SemaphoreWaitBody(S, lable) \
    { __asm \
        lable: MOV ACC, S \
               JZ lable \
               JB ACC.7, lable \
               DEC S \
      __endasm; }

#else   ///////////////////////////////////////////////////////////////// Non-Busy-waiting version ///////////

//Pending (Probably try thread-yield)
#define CNAME(s) _##s
#define SemaphoreCreate(s,n) \
    s = n; \
    s##Queue[0] = 0xFF; \
    s##Queue[1] = 0xFF; \
    s##Queue[2] = 0xFF; \
    s##IdxF = 0; \
    s##IdxR = 0;

#define SemaphoreSignal(s) SEMAPHORE_SIGNAL(s, CNAME(s))
#define SEMAPHORE_SIGNAL(s, var_sem) \
    __asm \
        inc var_sem \
    __endasm; \
    if (s <= 0) { \
        if ( toP == 1 ){ \
            TH0 = 0x1; \
            TL0 = 0x1; \
            temp = s##Queue[s##IdxF]; \
            s##IdxF = (s##IdxF + 1) % 3; \
            ThreadYield_extra(temp); \
        } \
        toP = 1; \
    } \
    if (empty == 2 & toP == 1) { \
        TH0 = 0x1; \
        TL0 = 0x1; \
        temp = s##Queue[s##IdxF]; \
        s##IdxF = (s##IdxF + 1) % 3; \
        ThreadYield_extra(temp); \
     }


// 若semaphore variable 減1後大於等於0, 繼續往後執行
// 若semaphore variable 減1後小於0, 則把目前的thread加到對應的 waiting queue (mutex, full, or empty queue),
// 並把等待的 (Consumer or Producer) thread count 加1,
// 然後儲存current thread 的 special function register 的狀態到對應的stack (SAVESTATE),
// 接著復原下一個thread 的之前的狀態
#define CONCAT( x, y ) x##y
#define SemaphoreWait(s) _SemaphoreWait(s, _##s, __COUNTER__)
#define _SemaphoreWait(sv, var_sem, lable1) _SemaphoreWait_(sv, var_sem, lable1)
#define _SemaphoreWait_(sv, var_sem, lable2) SemaphoreWaitBody(sv, var_sem, lable2##$)
#define SemaphoreWaitBody(sv, S, lable) \
    { __asm \
          DEC S \
          MOV ACC, S \
          JNB ACC.7, lable \
      __endasm; } \
     if ( (char *)&sv == 0x33 && currentThread == 0) \
         saveAddress(Consumer); \
     if ( (char *)&sv == 0x33 && currentThread == 1) \
         saveAddress(Producer1); \
     if ( (char *)&sv == 0x33 && currentThread == 2) \
         saveAddress(Producer2); \
     if ( (char *)&sv == 0x34) \
         saveAddress(Consumer); \
     if ( (char *)&sv == 0x35){ \
             saveAddress(Producer1); \
     } \
     TH0 = 0x1; \
     TL0 = 0x1; \
     ThreadYield(); \
     toP = 0; \
      __asm \
          lable: nop \
      __endasm;

#endif // BUSY_WAITING
#endif //__PREEMPTIVE_H__







