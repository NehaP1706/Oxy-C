## FCFS SCHEDULER:

```c
PID: 4 | creationTime: 84
PID: 5 | creationTime: 84
PID: 6 | creationTime: 84
PID: 7 | creationTime: 84
PID: 8 | creationTime: 84
PID: 9 | creationTime: 84
PID: 10 | creationTime: 84
PID: 11 | creationTime: 84
--> Scheduling PID 4 (lowest creation_time)
```

### Average: wait=40 runtime=13 turnaround=54

## CFS SCHEDULER:

```c
PID: 3 | vRuntime: 18
PID: 8 | vRuntime: 12
PID: 10 | vRuntime: 0
PID: 11 | vRuntime: 0
```
--> Scheduling PID 10 (lowest vRuntime)

### Average: wait=33 runtime=18 turnaround=51

## RR SCHEDULER:

```c
[CPU 0] Switching -> PID 4 (state=RUNNABLE)
[CPU 0] Returned   <- PID 4 (new state=RUNNABLE)
[CPU 0] Switching -> PID 5 (state=RUNNABLE)
[CPU 0] Returned   <- PID 5 (new state=RUNNABLE)
[CPU 0] Switching -> PID 6 (state=RUNNABLE)
[CPU 0] Returned   <- PID 6 (new state=RUNNABLE)
[CPU 0] Switching -> PID 7 (state=RUNNABLE)
[CPU 0] Returned   <- PID 7 (new state=RUNNABLE)
[CPU 0] Switching -> PID 8 (state=RUNNABLE)
[CPU 0] Returned   <- PID 8 (new state=RUNNABLE)
[CPU 0] Switching -> PID 9 (state=RUNNABLE)
[CPU 0] Returned   <- PID 9 (new state=RUNNABLE)
[CPU 0] Switching -> PID 10 (state=RUNNABLE)
[CPU 0] Returned   <- PID 10 (new state=RUNNABLE)
[CPU 0] Switching -> PID 11 (state=RUNNABLE)
[CPU 0] Returned   <- PID 11 (new state=RUNNABLE)
[CPU 0] Switching -> PID 4 (state=RUNNABLE)
[CPU 0] Returned   <- PID 4 (new state=RUNNABLE)
[CPU 0] Switching -> PID 5 (state=RUNNABLE)
[CPU 0] Returned   <- PID 5 (new state=RUNNABLE)
[CPU 0] Switching -> PID 6 (state=RUNNABLE)
[CPU 0] Returned   <- PID 6 (new state=RUNNABLE)
[CPU 0] Switching -> PID 7 (state=RUNNABLE)
[CPU 0] Returned   <- PID 7 (new state=RUNNABLE)
[CPU 0] Switching -> PID 8 (state=RUNNABLE)
[CPU 0] Returned   <- PID 8 (new state=RUNNABLE)
[CPU 0] Switching -> PID 9 (state=RUNNABLE)
[CPU 0] Returned   <- PID 9 (new state=RUNNABLE)
[CPU 0] Switching -> PID 10 (state=RUNNABLE)
[CPU 0] Returned   <- PID 10 (new state=RUNNABLE)
[CPU 0] Switching -> PID 11 (state=RUNNABLE)
[CPU 0] Returned   <- PID 11 (new state=RUNNABLE)
```
### Average: wait=3 runtime=22 turnaround=26


## IMPLEMENTATION SPECIFICATIONS:

1) FCFS SCHEDULER:
- Only parameter of a process to be tracked is the creation time. (Added a struct element in proc.h)
- No pre-emption required at any time. (Does not yield() in trap.c)
- Loop through all the currently running processes and find the process with minimum creation_time, that process it to be scheduled next. (Context switch in proc.c)

2) CFS SCHEDULER:
- Parameters of a process to be tracked: nice value, weight, vruntime, allowed_slice among others. (elements added in proc.h proc struct)
- If the process has run for longer than the allowed slice, we increment the vruntime and yield(). (trap.c modifications)
- Loop through all the currently running processes and find the process with minimum vruntime, that process it to be scheduled next. (Context switch in proc.c)
- Assuming the set_nice syscall was not expected to be implemented (not mentioned in the doc), settings for randomization have been commented out.

3) RR SCHEDULER:
- Already implemented by cloned system.

4) readcount.c and readcount.h
- Global variable 'global_read_bytes' is initialized to 0 and maintained across all processes.
- Auxilary helper functions called in sysfile.c to update the said variable and make it accessible to both kernel and user.
- The bytes read from the terminal input are also added as the system treats it like a file_read. (AMBIGUOUS NATURE)

## EXTRAS:

5) schedtest.c
- A C program that spawns child processes to burn up the CPU usage.
- Prints out the average runtime, waitime and turnaround time.
- Usage: schedtest.c nchildren maxwork

6) waitx() and wait()
- For the kernel and user to have synchronized startime, endtime information we required another syscall.
- Synchronizes the setting of variables in the schedtest.c and proc.c (for process variables).


