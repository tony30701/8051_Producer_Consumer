/* file: testpreempt.c */

#include <8051.h>
#include "preemptive.h"
#define N 3

// 30H ~ 7FH : general purpose register
__idata __at(0x30) char buffer[3];
__idata __at(0x33) char mutex;
__idata __at(0x34) char full;
__idata __at(0x35) char empty;
__data __at(0x36) char letter;
__data __at(0x37) char rear  = 0;
__data __at(0x38) char front = 0;

__data __at (0x39) char thread_empty;
__data __at (0x3A) char temp = 0;
__data __at (0x3B) char number = 0;
__data __at (0x3C) char toP = 0;
__data __at (0x5D) unsigned char N_delay;
__data __at (0x5E) unsigned char counterISR;

__data __at (0x70) unsigned char mutexQueue[3];
__data __at (0x73) unsigned char mutexIdxF;
__data __at (0x74) unsigned char mutexIdxR;
__data __at (0x75) unsigned char fullQueue[3];
__data __at (0x78) unsigned char fullIdxF = 0;
__data __at (0x79) unsigned char fullIdxR = 0;
__data __at (0x7B) unsigned char emptyQueue[3];
__data __at (0x7E) unsigned char emptyIdxF;
__data __at (0x7F) unsigned char emptyIdxR;

void Producer1(void);
void Producer2(void);
void Consumer(void);
extern ThreadID currentThread;
extern char savedSP[MAXTHREADS];

void saveAddress(FunctionPtr fp){
    if (fp == Consumer){
        fullQueue[fullIdxR] = currentThread;
        fullIdxR = (fullIdxR + 1) % 3;
    } else if (fp == Producer1 || fp == Producer2) {
        emptyQueue[emptyIdxR] = currentThread;
        emptyIdxR = (emptyIdxR + 1) % 3;
    } else {
        mutexQueue[emptyIdxR] = currentThread;
        mutexIdxR = (emptyIdxR + 1) % 3;
    }

}

/**********
void saveAddress(FunctionPtr fp){
     DPL = (unsigned int)fp & 0xff;
     DPH = (unsigned int)fp >> 8;
     if (fp == Consumer){
         if (fullCount == 0){
             fullQueue[0] = DPH;
             fullQueue[1] = DPL;
         } else {
             fullQueue[2] = DPH;
             fullQueue[3] = DPL;
         }
     } else if (fp == Producer) {
         emptyQueue[0] = DPH;
         emptyQueue[1] = DPL;
     } else {
         mutexQueue[0] = DPH;
         mutexQueue[1] = DPL;
     }
} ************/


//time unit: 1ms
void delay(unsigned char n){

    N_delay = n;

    /* **********************************************
     * Assembly code for doing nothing for 1ms
     * 1ms 需要 ~921(2 + 2 + 2*210 + 2*250 + 2) 個 machine cycle
     * Instruction      Machine cycle
     * MOV -, -     =>  2 * 1 = 2
     * DJNZ -, -    =>  2 * 1 = 2
     * RET          =>  2 * 1 = 2
     ************                                    ***********/
    __asm
               MOV R4, CNAME(N_delay)
        DELAY: MOV R2, #229
               MOV R3, #229
        HERE:  DJNZ R2, HERE
        AGAIN: DJNZ R3, AGAIN
               DJNZ R4, DELAY
               RET
    __endasm;
}


/***************************************************************************
 * where they access the shared variables, 
 * you can surround the code fragment using the __critical { } construct, 
 * to ensure that the two shared vars are accessed atomically.  
 * 
 * Date: 21/12/2018
 ****************************************************************************/

void Producer1(void) {

    char id = 0x01;

    letter = 0x41;  
    while(1){

        while(empty==0){}
        SemaphoreWait(empty);

		//add the new char to the buffer
		__critical{
            SemaphoreWait(mutex);
			buffer[rear] = (char)letter;
			rear = (rear + 1) % N;
		    SemaphoreSignal(mutex);
        }
		SemaphoreSignal(full);

		letter = (char)(letter + 1);
		if (letter == 0x5B)
			letter = 0x41;

        delay(5);        
    }
}

void Producer2(void) {

    char id = 0x02;

    number = 48;
    while(1){

        SemaphoreWait(empty);
        __critical{
            SemaphoreWait(mutex);
            buffer[rear] = (char)number;
            rear = (rear + 1) % N;
            SemaphoreSignal(mutex);
        }
        SemaphoreSignal(full);

        number = number + 1;
        if (number == 58)
           number = 48;
        delay(5);
    }
}


/*******************************************************************
 * In your Consumer() code, where it sets TMOD, 
 * you might want to do TMOD |= 0x20;
 * instead of TMOD = 0x20, because TMOD is also assigned 
 * by the (modified) Bootstrap code 
 * to set up the timer interrupt in timer-0 for preemption.  
 * This way, it preserves the Bootstrap code’s setting.
 *
 * Date: 21/12/2018
 *************************************************************************/

void Consumer(void) {

    TMOD |= 0x20;
    TH1 = -6;
    SCON = 0x50;
    TR1 = 1;

    delay(9);
    while(1){
        //不可以被搶先, Consumer 想用CPU時, 就讓Consumer 佔用的時間結束
        while (full==0){}

		SemaphoreWait(full);
		// remove the next char from the buffer
        __critical{
		    SemaphoreWait(mutex);
		    SBUF = buffer[front];
            while(!TI){}
            TI = 0;
		    SemaphoreSignal(mutex);
         }
		SemaphoreSignal(empty);
		front = (front + 1) % N;
    }
}

void main(void) {
    rear = front = 0;
    buffer[0] = buffer[1] = buffer[2] = 0;
    SemaphoreCreate(mutex, 1);
    SemaphoreCreate(full, 0);
    SemaphoreCreate(empty, 3);
    ThreadCreate(Producer1);
    ThreadCreate(Producer2);
    Consumer();
}

void _sdcc_gsinit_startup(void) {
     __asm
             ljmp  _Bootstrap
     __endasm;
}

/**************************************************************************************
 *
 * As explained in lecture, the ISR should be defined in the same file as main() 
 * in order for SDCC to generate the proper code for ISR.  
 * So, include the following lines at the bottom of your testpreempt.c:
 *
 * This allows the ISR to call your routine named myTimer0Handler 
 * (defined in preemptive.c) to handle the actual interrupt itself.
 *
 * Date: 21/12/2018
 **************************************************************************************/

void _mcs51_genRAMCLEAR(void) {}
void _mcs51_genXINIT(void) {}
void _mcs51_genXRAMCLEAR(void) {}

void timer0_ISR(void) __interrupt(1) {
        __asm
                ljmp _myTimer0Handler
        __endasm;
}

