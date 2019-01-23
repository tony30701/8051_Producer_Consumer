/* file: testpreempt.c */

#include <string.h>
#include <math.h>
#include <float.h>
#include <8051.h>
#include "preemptive.h"

#define N 2
#define CARS 5


__code __at (0x900) char car[4] = {'C','a','r', '\0'};
__code __at (0x910) char got[4] = {'g', 'o', 't', '\0'};
__code __at (0x920) char spot[5] = {'s', 'p', 'o', 't', '\0'};
__code __at (0x930) char space[2] = {' ', '\0'};
__code __at (0x940) char left_brace[2] = {'(', '\0'};
__code __at (0x950) char right_brace[3] = {')', '.', '\0'};
__code __at (0x960) char comma[2] = {',', '\0'};
__code __at (0x970) char ms[3] = {'m', 's', '\0'};
__code __at (0x980) char left[5] = {'l', 'e', 'f', 't', '\0'};

// 30H ~ 7FH : general purpose register
__data __at (0x30) char spots[N];
__data __at (0x32) char mutex;          // mutex 存在的原因: 避免spots[]被同時寫入嗎？ 那還需要加__critical{} 嗎？
__data __at (0x33) char spot_empty;
__data __at (0x34) char thread_empty;   // thread_empty 使用data & char, idata也沒發現問題
__data __at (0x35) char rear;

__data __at (0x2A) unsigned char spot_car1;
__data __at (0x2B) unsigned char time_start_car1;
__data __at (0x2C) unsigned char time_end_car1;
__data __at (0x2D) unsigned char spot_car2;
__data __at (0x2E) unsigned char time_start_car2;
__data __at (0x2F) unsigned char time_end_car2;
__data __at (0x37) unsigned char spot_car3;
__data __at (0x38) unsigned char time_start_car3;
__data __at (0x39) unsigned char time_end_car3;
__data __at (0x3A) unsigned char spot_car4;
__data __at (0x3B) unsigned char time_start_car4;
__data __at (0x3C) unsigned char time_end_car4;
__data __at (0x3D) unsigned char spot_car5;
__data __at (0x3E) unsigned char time_start_car5;
__data __at (0x3F) unsigned char time_end_car5;

__data __at (0x5D) unsigned char N_delay;
__data __at (0x5E) unsigned char counterISR;

// Keep for the version using same Car function
// __data __at (0x38) char carID = 1;
// __data __at (0x39) char temp;

extern char currentThread;
extern char bitmap;

unsigned char now(void){
    
    unsigned char time = 9 * counterISR;

    unsigned char th = TH0;
    unsigned char bit;
    unsigned char shift_t = 0;
    //__data __at (0x4D) float sum = 0;
    //__data __at (0x4E) float power;

    /******
    shift = 0;
    do {
        bit = (tl >> shift) & 0x1;
        if (bit){
            power = 1;
            for (i=0; i<shift; i++){
                power = 2*power;
            }
            sum += power;
        }
        shift++;
    }while(shift <= 4);  ********/

    /*****
    shift = 0;
    do {
        bit = (th >> shift) & 0x1;
        if (bit){
            power = (float)2*2*2*2*2;
            for (i=0; i<shift; i++){
                power = 2*power;
            }
            sum += power;
        }
        shift++;
    }while(shift <= 7);  

    bit = __fs2uchar(sum)   *******/;
  
    do {
        bit = (th >> shift_t) & 0x1;
        if (bit){
            if (shift_t == 5){
                //time = time + 1;
                __asm 
                    mov a,#0x1
                    add a,r7
                    mov r7,a
                __endasm;
            }
            if (shift_t == 6){
                //time = time + 2;
                __asm 
                    mov a,#0x2
                    add a,r7
                    mov r7,a
                __endasm;
            }
            if (shift_t == 7){
                //time = time + 4;
                __asm 
                    mov a,#0x4
                    add a,r7
                    mov r7,a
                __endasm;
            }
        }
        shift_t++;
    }while(shift_t <= 7);

    
    return time;
}


//cristal oscilator frequency: 11.059 MHz
//Timer 0 Mode 0 (13bit) : 2^13=8192 machine cycles
//8192 * 1.085us = 8888.32us = 8.88832ms

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

//time unit: 10us
void delay_10us(unsigned char n){

    N_delay = n;
    __asm
                MOV R3, CNAME(N_delay)
        DELAY2: MOV R2, #3
        HERE2:  DJNZ R2, HERE2
                DJNZ R3, DELAY2
                RET
    __endasm;

}

/****************
//使用同一個函式會有點麻煩, 因為compiler無法判別哪一個thread要使用那一個bank
//導致於AR0~AR7 & R0~R7 混用(AR代表絕對位址)
void Car(void) {

	// Serial Port Setting
	TMOD = 0x20;
	TH1 = -6;
	SCON = 0x50;
	TR1 = 1;

    __asm
        mov r6,_carID
        mov r0,#_carID
        inc @r0
    __endasm;

    while(1){

        /////////////////////////////////////////////////////////
        //[1] 確認有沒有空的 parking lot
        SemaphoreWait(empty);

        //[2] 拿到停車位進來停車, 1次只能有1台車進來選位置, mutual exclusion
		//add the new char to the buffer
        SemaphoreWait(mutex);
		spots[rear] = (char)carID;
		rear = (rear + 1) % N;

        //[2-1]停好車子, 釋放lock, 讓下一台車子進來選位置
		SemaphoreSignal(mutex);
		
        //[3] 停了亂數時間後, 離開停車場
        delay_10us(10);
        SemaphoreSignal(full);
        //////////////////////////////////////////////////////////

        //Debug Breakpoint	
        //Assembly code for SBUF = 48 + car
		__critical{
		    __asm
                mov a,r6
                mov r5, a
                mov _temp, a
                mov a,#0x30
                add a,r5
                mov _SBUF,a
            __endasm;
		    while(!TI){}
        }
        TI = 0; 
        delay_10us(1);

        //[4] 結束這個執行緒, 讓下一個執行緒被建立
        //carID: 0x30, 0x31, 0x32, 0x33, 0x345
        //ThreadExit();
            
    }
}                                                             ***********/


void Car1(void){

    char car = 1;

	// Serial Port Setting
	TMOD = 0x20;
	TH1 = -6;
	SCON = 0x50;
	TR1 = 1;

    while(1){
        
        //[1]確認有沒有車位 SemaphoreWait(spot_empty)
        //   if yes, take mutex lock, 停車並且記錄時間; 
        //       同一時間停車場只能有一台車在停車 SemaphoreWait(mutex_lock)
        //       __critical{} 似乎不需要
        //   if no, wait in a spin lock of semaphore
        SemaphoreWait(spot_empty);
        SemaphoreWait(mutex);
        spots[rear] = 1;
        spot_car1 = rear;
        rear = (rear + 1) % N;
        SemaphoreSignal(mutex);
        time_start_car1 = now();     //記錄停車的起始時間

        /***
		//Debug Breakpoint
        __critical{
		    SBUF = 48 + 1;
		    while(!TI){}
        }
		TI = 0;    ****/ 

        //[2]這台車放在停車場的時間
        delay(10);

        //[3]時間到, 離開停車場, 並記錄離開的時間
        SemaphoreSignal(spot_empty);
        time_end_car1 = now();
        ThreadExit();
        while(1){}    
    }
}

void Car2(void){

    char car = 2;

	// Serial Port Setting
	TMOD = 0x20;
	TH1 = -6;
	SCON = 0x50;
	TR1 = 1;

    while(1){

        //[1]確認有沒有車位 SemaphoreWait(spot_empty)
        //   if yes, take mutex lock, 停車並且記錄時間;
        //       同一時間停車場只能有一台車在停車 SemaphoreWait(mutex_lock)
        //       __critical{} 似乎不需要
        //   if no, wait in a spin lock of semaphore
        SemaphoreWait(spot_empty);
        SemaphoreWait(mutex);
        spots[rear] = 2;
        spot_car2 = rear;
        rear = (rear + 1) % N;
        SemaphoreSignal(mutex);
        time_start_car2 = now();     //記錄停車的起始時間

        //[2]這台車放在停車場的時間
        delay(5);

        //[3]時間到, 離開停車場, 並記錄離開的時間
        SemaphoreSignal(spot_empty);
        time_end_car2 = now();
        ThreadExit();
        while(1){}
    }
}

void Car3(void){

    char car = 3;

	// Serial Port Setting
	TMOD = 0x20;
	TH1 = -6;
	SCON = 0x50;
	TR1 = 1;
 
    while(1){

        //[1]確認有沒有車位 SemaphoreWait(spot_empty)
        //   if yes, take mutex lock, 停車並且記錄時間;
        //       同一時間停車場只能有一台車在停車 SemaphoreWait(mutex_lock)
        //       __critical{} 似乎不需要
        //   if no, wait in a spin lock of semaphore
        SemaphoreWait(spot_empty);
        SemaphoreWait(mutex);
        spots[rear] = 3;          //停車
        spot_car3 = rear;
        rear = (rear + 1) % N;    
        SemaphoreSignal(mutex);   //我停好車了, 通知下一台車子進來停車
        time_start_car3 = now();     //記錄停車的起始時間

        //[2]這台車放在停車場的時間
        delay(2);

        //[3]時間到, 離開停車場, 並記錄離開的時間
        SemaphoreSignal(spot_empty);
        time_end_car3 = now();
        ThreadExit();        
        while(1){
            __asm
                nop
                nop
            __endasm;
        }
    }
}

void Car4(void){

	// Serial Port Setting
	TMOD = 0x20;
	TH1 = -6;
	SCON = 0x50;
	TR1 = 1;
 
    while(1){

        //[1]確認有沒有車位 SemaphoreWait(spot_empty)
        //   if yes, take mutex lock, 停車並且記錄時間;
        //       同一時間停車場只能有一台車在停車 SemaphoreWait(mutex_lock)
        //       __critical{} 似乎不需要
        //   if no, wait in a spin lock of semaphore
        SemaphoreWait(spot_empty);
        SemaphoreWait(mutex);
        spots[rear] = 4;          //停車
        spot_car4 = rear;
        rear = (rear + 1) % N;
        SemaphoreSignal(mutex);   //我停好車了, 通知下一台車子進來停車
        time_start_car4 = now();     //記錄停車的起始時間

        //[2]這台車放在停車場的時間
        delay(2);

        //[3]時間到, 離開停車場, 並記錄離開的時間
        SemaphoreSignal(spot_empty);
        time_end_car4 = now();
        ThreadExit();
        while(1){
            __asm
                nop
                nop
            __endasm;
        }
    }
}

void Car5(void){

	// Serial Port Setting
	TMOD = 0x20;
	TH1 = -6;
	SCON = 0x50;
	TR1 = 1;

    while(1){

        //[1]確認有沒有車位 SemaphoreWait(spot_empty)
        //   if yes, take mutex lock, 停車並且記錄時間;
        //       同一時間停車場只能有一台車在停車 SemaphoreWait(mutex_lock)
        //       __critical{} 似乎不需要
        //   if no, wait in a spin lock of semaphore
        SemaphoreWait(spot_empty);
        SemaphoreWait(mutex);
        spots[rear] = 5;          //停車
        spot_car5 = rear;
        rear = (rear + 1) % N;
        SemaphoreSignal(mutex);   //我停好車了, 通知下一台車子進來停車
        time_start_car5 = now();     //記錄停車的起始時間

        //[2]這台車放在停車場的時間
        delay(4);

        //[3]時間到, 離開停車場, 並記錄離開的時間
        SemaphoreSignal(spot_empty);
        time_end_car5 = now();
        ThreadExit();
        while(1){
            __asm
                nop
                nop
            __endasm;
        }
    }
}

void print_str(char *str){

    char i = 0;
    do{
        if(str[i] == '\0')
            break;
        __critical{
            SBUF = str[i];
            while(!TI){}
        }
        TI = 0;
        i = i + 1;
        delay(2);
    }while(1);
}

void print_char(char info){

    __critical{
        SBUF = 48 + info;
        while(!TI){}
    }
    TI = 0;
    delay(1);
}

void print_time(unsigned char time){

    
    char i = 2;
    unsigned char remainder = 0;
    unsigned char quotient = time;
    __data __at (0x6D) unsigned char num[3] = {0,0,0}; 
    do{
        remainder = quotient % 10;
        quotient = quotient / 10; 
        num[i--] = remainder;
        if (quotient <= 0){
            break;
        }
    }while(1);
    
    for (i=0; i<3; i++){
        print_char(num[i]);
        delay(2);
    }
}


void main(void) {

    char i;
    unsigned char spot_car;
    unsigned char time_start;
    unsigned char time_end;
    //carID = 1;  //keep for the version using same Car function

	// Serial Port Setting
	TMOD = 0x20;
	TH1 = -6;
	SCON = 0x50;
	TR1 = 1;

    //總共有 2個 parking lot 可使用
    SemaphoreCreate(mutex, 1);
    SemaphoreCreate(spot_empty, N);

    //總共有 4個thread semaphore, main function 佔用了1個
    SemaphoreCreate(thread_empty, 3);

    /****************************************
    //ThreadCreate Version1 for using same Car function
    for(i=0; i<CARS; i++){
        if(i==3){
            P1=0x99;
        }
        SemaphoreWait(thread_empty)
        ThreadCreate(Car);
    }                                   *****/


    //ThreadCreate Version2 for using separate Car function
    SemaphoreWait(thread_empty)
    ThreadCreate(Car1);

    SemaphoreWait(thread_empty)
    ThreadCreate(Car2);

    SemaphoreWait(thread_empty)
    ThreadCreate(Car3);

    SemaphoreWait(thread_empty)
    ThreadCreate(Car4);

    SemaphoreWait(thread_empty)
    ThreadCreate(Car5);

    while(1){

		if (bitmap == 0x01){
            for(i=0; i<CARS; i++){
                switch(i){
                    case 0:
                        spot_car = spot_car1; time_start = time_start_car1; time_end = time_end_car1;
                        break; 
                    case 1:
                        spot_car = spot_car2; time_start = time_start_car2; time_end = time_end_car2;
                        break; 
                    case 2:
                        spot_car = spot_car3; time_start = time_start_car3; time_end = time_end_car3;
                        break; 
                    case 3:
                        spot_car = spot_car4; time_start = time_start_car4; time_end = time_end_car4;
                        break; 
                    case 4:
                        spot_car = spot_car5; time_start = time_start_car5; time_end = time_end_car5;
                        break; 
                    default:
                        spot_car = 0;  time_start = 0;   time_end = 0;
                        break;      
                }
                //Car got spot (start_time, spot).
				print_str(car);
                print_char(i+1);
                print_str(space);

                print_str(got);
                print_str(space);

                print_str(spot);
                print_str(space);

                print_str(left_brace);
                print_time(time_start);
                print_str(ms);
                print_str(comma);
                print_str(space);
                print_char(spot_car);        
                print_str(right_brace);
                print_str(space);
                
                //Car left spot (exit_time).
				print_str(car);
                print_char(i+1);
                print_str(space);

                print_str(left);
                print_str(space);

                print_str(spot);
                print_str(space);

                print_str(left_brace);
                print_time(time_end);
                print_str(ms);
                print_str(right_brace);
                print_str(space);
                print_str(space);
	        }            
        }
        delay(1);
    } 
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

