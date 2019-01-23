# 8051 simulated by EdSim (Checkpoint 5)
###### tags: `8051` , `EdSim51`, `SDCC`, `preemptive`, `ISR`
\
For this programming project checkpoint, I am going to 
(1) add a delay(n) function to preemptive multithreading and semaphore code, 
(2) ensure threads can terminate so its space can be recycled by another thread. 
(3) test delay function and thread recycling based on  the parking lot example

\-
### [1] delay(n), now()
The void delay(unsigned char n) and unsigned char now(void) functions are implemented. The former delays the thread by n time units; the latter returns the “current time” (i.e., number of time units that has elapsed since the system reboots).  There are a number of considerations:
* in python, the time unit for thread.sleep() is in seconds, but it can be a floating point.  On 8051, we limit the time unit to be an unsigned char (0-255), but what is a good time unit? (e.g., number of seconds, ms, µs, or some multiple of the 8051 timer?)   Note that any time unit that is larger than 1 second is not useful.
* An important consideration is that delay() is not an exact delay but is a delay for “at least n time units” and “less than (n + 0.5) time units” for it to be acceptable (otherwise, it rounds up to n+1 time units, which would not be correct).  Of course, the more accurate the better, but there is an inherent limit on how accurate it can be.
* Based on the above requirement, state the choice of time unit and provide the justification for how to think a delay() can be implemented to meet the requirement above.

Explain how to implement the delay function in relation to the timer used for preemption.  By default, we set it up for a 13-bit timer for the quantum. Considering that each thread gets its own delay call independently:

* what does timer-0 ISR have to do to support these multiple delays and now()?  
* what if all threads call delay() and happen to finish their delays all at the same time?  How can programmer ensure the accuracy of delay? (i.e., between n and n+0.5 time units)?  
* How does the worst-case delay completion (i.e., all threads finish delaying at the same time) affect choice of time unit?

Assume it is okay for now() to wrap around to 0 after it exceeds 255 time units.
Create specific test case to ensure delay function works before proceeding to the next part.

\-
### [2] Robust Thread Termination and Creation

Modify ThreadExit() code so that a thread can safely terminate by either calling ThreadExit() or return normally from the function.  This is easy, because all programmers have to do is to push the return address of ThreadExit() on the stack.  ThreadExit() just has to mark the thread as unallocated.
 
One other thing programmers need to do is to guard thread creation and termination.  We have a maximum limit of 4 threads.  This means programmers need to use a semaphore to allow creation of threads up to the max, and any attempt to create additional threads will block until some thread has exited.  Add the proper code in the ThreadCreate() and ThreadExit() code to make use of the semaphore(s) similar to the bounded-buffer example.

One thing about ThreadExit() is that if there exists the last thread to exit, then it should enter an infinite loop, instead of returning to nowhere.

\-

### [3] Parking Lot Example

Considering the parking simulation example, make 5 cars competing for 2 spots.  Make thread for each car.  However, since the threads package supports at most 4 threads (including main), the main() will use a semaphore, similar to the bounded-buffer example to create threads up to the maximum number available.
 
Instead of immediately writing out the result, programmers should maintain a “log” for the events:
when a car gets the parking spot (what time, which spot)
when a car exits the parking lot (what time)
Minimally, show the memory dump of the table to reflect the content of this log.
 
==**[Extra task]** Display the output of the log to UART in a human-readable text format.==




\
\
**補充資訊:**

■ Timer programming using Timer interrupts involves following algorithm.
(Ref: https://www.engineersgarage.com/tutorials/timers-8051-timer-programming-tutorial )

1. Configure the Timer mode by passing a hex value to TMOD register. 
2. Load the initial values in the Timer low TLx and high THx byte.
3. Enable the Timer interrupt by passing hex value to IE register or setting required bits of IE register. For example,
![ ](https://i.imgur.com/0NNS6kJ.png =300x150)


4. Start the Timer by setting TRx bit.
5. Write Interrupt Service Routine (ISR) for the Timer interrupt. For example,
![](https://i.imgur.com/peO82l5.png)

6. If the Timer has to be stopped after once the interrupt has occurred, the ISR must contain the statement to stop the Timer. For example,
```clike=
void ISR_Timer1(void) interrupt 3
{
    <Body of ISR>
    TR1 =0;
}
```
7. If a routine written for Timer interrupt has to be repeated again and again, the Timer run bit need not be cleared. But it should be kept in mind that Timer will start updating from 0000H and not the initial values in case of mode 0 and 1. So the initial values must be reloaded in the interrupt service routine.
	For example,
```clike=
void ISR_Timer1(void) interrupt 3
{
    <Body of ISR>
    TH1 = 0XFF;   //load with initial values if in mode 0 or 1
    TL1 = 0xFC;
}
```
\-
**Time 0 Mode 0(13bit)**
![](https://i.imgur.com/5EC7WFn.png =550x140)

\
\-
  
■ TIME DELAY CALCULATION IN 8051
Ref: http://iamtechnical.com/time-delay-calculation-in-8051

To make the 8051 system compatible with the serial port of the personal computer PC, 11.0592MHz crystal oscillators is used.
In the 8051, one machine cycle lasts 12 oscillator periods. So to calculate the machine cycle, we take 1/12 of the crystal frequency, then take the inverse of it results in time period. i.e frequency = 1/time period.

Example # 5:
For a crystal frequency of 11.0592 MHz, lets find the time delay in the following 
subroutine. The machine cycle is 1.085 us.

==**Answer:**==

![](https://i.imgur.com/Uelz7NS.png =350x180)

'HERE' Loop Calculations: 1+1+2, so [(1+1+2)x250] x 1.085 us = 1085 us.

'AGAIN' Loop Calculations: In this loop "MOV R3,#250" and "DJNZ R2,AGAIN" at the begining and end of the AGAIN loop add [(1+2)x200]x1.085 us = 651us to the time delay. The AGAIN loop repeats the HERE loop 200 times so 200x1085 us = 217000 us. As a result the total time delay will be  217000 us + 651 us = 217651 us or 217.651 milliseconds. The time is approximate as we have ignored the first and the last instructions in the 
subroutine i.e. DELAY: "MOV R2,#200" and "RET".


