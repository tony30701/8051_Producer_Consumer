# 8051_Producer_Consumer Programming (Checkpoint 3)
###### tags: `8051` , `EdSim51`, `SDCC`, `preemptive`, `ISR`

For this programming project checkpoint, I am going to implement 
(1) semaphore with busy wait for preemptive multithreading (add the semaphores in preemptive.h) and 
(2) test the code using the classical producer-consumer bounded-buffer example (revise the previous producer-consumer example in Checkpoint 2 by semaphore primitives).
 
### [1] preemptive.h (and preemptive.c if necessary): semaphores
Add a semaphore API to preemptive.h and preempt.c if necessary, depending on whether you implement it as a macro or as a function.  A macro simply defines the inline assembly code and gives you precise control over the implementation; so we will assume a macro implementation, although you may also choose code implementation.

The basic semaphore API to implement consists of

```clike=
#define SemaphoreCreate(s, n)   // create a counting semaphore s initialized to n
#define SemaphoreWait(s)        // do busy-wait on semaphore s
#define SemaphoreSignal(s)      // signal a semaphore s
```

**1.1  SemaphoreCreate(s, n)**

SemaphoreCreate simply initializes the semaphore s (an “integer”, but we will just use a char) to the value n.  In the busy-waiting version, that is all it takes.  It can be writen as a macro in C or as inlined assembly.
 
If choosing to write this as a function in C, you will need to pass the pointer to the semaphore.  However, it is not recommended, also because passing multiple parameters including pointers can get complicated in SDCC.


[Extra task]  Non-busy-waiting version
It should initialize associated data structures, like the list of threads that are blocked on this semaphore.


**1.2  SemaphoreSignal(s)**

SemaphoreSignal() is very simple for the busy-waiting version of semaphore: simply increment the semaphore variable.  It is recommended that you write this in assembly, because you can do this as an atomic operation (i.e., single assembly instruction), namely INC.  The operand itself can refer to the symbol by prepending it with an underscore (_).  However, if you define it as a macro argument, e.g., #define SemaphoreSignal(s), if you write _s, the s part will not get expanded into the actual name of the semaphore!  Fortunately, C macro preprocessor includes the ## operator, which lets you concatenate two symbols together. For convenience, you can define another macro for converting a C name into an assembly name:
#define CNAME(s) _ ## s

This way, if you write CNAME(mutex), it gets macro-expanded into _mutex, which is exactly what you want.


[Extra Credit] Non-busy-waiting version
It needs to go through the waiting list and pick one process to wake up.




**1.3  SemaphoreWait(s)**

This one is a bit more work, but still not too difficult.  The pseudocode looks like
```clike=
Wait(S) :
   while (S <= 0) { }  //busy wait
   S--;
```
One thing to notice is that since the value of S is expected to drop below zero, do not declare the semaphore as an unsigned!

You could write this in C, but SDCC might not compile it to the right code.  Look at the .asm file to see how it compiles the code.  If you can get it working properly in C, then that is fine. However, it is recommended that you write assembly code for this.

The assembly code looks something like this:
```clike=
#define SemaphoreWaitBody(S, label) \
    { __asm \
      label: ;; top of while-loop \
             ;; read value of _S into accumulator (where S is the semaphore) \
             ;; use conditional jump(s) to jump back to label if accumulator <= 0  \
             ;; fall-through to drop out of while-loop \
      dec  CNAME(S) \
      __endasm; }
```
The list of conditional branch instructions in 8051 are
```
JZ  label        ;; jump if accumulator is zero
JNZ label        ;; jump if accumulator is not zero
JB bit, label    ;; jump if bit is 1
JNB bit, label   ;; jump if bit is 0
```

Hint: if ACC.7 bit (the sign bit) is 1, then the number in the accumulator is negative.
 
However, there is one issue with the assembly label.  Because we are defining it as a macro, and we use multiple semaphores in the same code, we need to give each instance a different label, so the instruction can jump to the proper label.  However, we don’t want the caller to have to come up with a unique label each time they call SemaphoreWait.  There is one simple solution: C preprocessor defines the \_\_COUNTER\_\_ name, which will generate a new integer each time it is used.  In SDCC, user labels in the form of 1$, 2$, … up to 100 can be used without conflict with the assembler’s labels.  So we can define SemaphoreWait to pass a unique label based on \_\_COUNTER\_\_ to SemaphoreWaitBody each time.  Just be sure you concatenate \_\_COUNTER\_\_ with the $ to form the label.


[Extra Credit]  The non-busy-waiting version will need to put the thread to a list of waiting processes, instead of busy waiting.  But if the thread does not have to wait then it just does the decrement.  Note that at the time of getting waken, the semaphore value can still be negative!  So this means you don’t keep testing the while (s <= 0); when you are waken, you just decrement the s anyway, even if s is still negative!

!!!


### [2] testpreempt.c

testpreempt.c can be based on the file in CheckPoint2.   The difference are to replace the test case with the bounded-buffer (3-deep) instead of the single buffer that you have been using.


**2.1  Process Synchronization**

Declare three semaphores mutex, full, and empty at known locations.  Add code to Create and initialize them accordingly.

Declare a 3-deep char buffer, and you need to keep track of the head and tail for this circular queue -- either as index into the array or as pointer.  Initialize them accordingly -- in your code, not in the global declaration.


**2.2  Producer**

The producer “produces” one character at a time starting from ‘A’ to ‘Z’ and start over again.  It needs to loop forever
```cpp=
WaitSemaphore(empty);
WaitSemaphore(mutex);
// add the new char to the buffer
SignalSemaphore(mutex);
SignalSemaphore(full);
```
You are to write the code to add the new char to the buffer and generate the next char outside.  Note: keep the critical section as short as possible!


**2.3  Consumer**

The consumer consumes one char at a time from the buffer but otherwise should behave the same as before.  That is, it needs to loop forever
```cpp=
WaitSemaphore(full);
WaitSemaphore(mutex);
// remove the next char from the buffer
SignalSemaphore(mutex);
SignalSemaphore(empty);
```
Similarly, keep the critical section as short as possible!


!!!


**[ ] Debug from deriving initial flow**
```
main() -----> Consumer() -------> SemaphoreWait(full) (full = full-1 = -1(0xFF); buffer沒有東西, add thread to waiting queue) 
                         -------> saveAddress(Consumer): thread 0 放入 fullQueue[0]; fullIdxR = 0 + 1 = 1 
                         -------> ThreadYield(): 交出 CPU 的控制權

       -----> Producer() -------> buffer[0] = letter(0x41); rear = 0 + 1 = 1 
                         -------> letter = letter + 1 = 0x42
                         -------> SemaphoreSignal(full) (buffer已經有東西, full = full+1 = 0 (S<=0), 有thread在等待, wakeup thread 0)
                                  ------> SAVESTATE: 儲存 Producer 的 register state 到 Stack
                                  ------> currentThread = fullQueue[fullIdxF=0]; fullIdxF = 0 + 1 = 1 
                                  ------> RESTORESTATE: 復原 thread 0 的 register state by stack info
                                  ------> ret:  

```
![](https://i.imgur.com/0WHNpcG.png)



