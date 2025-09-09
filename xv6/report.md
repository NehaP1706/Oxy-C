FCFS SCHEDULER:

PID: 4 | creationTime: 84
PID: 5 | creationTime: 84
PID: 6 | creationTime: 84
PID: 7 | creationTime: 84
PID: 8 | creationTime: 84
PID: 9 | creationTime: 84
PID: 10 | creationTime: 84
PID: 11 | creationTime: 84
--> Scheduling PID 4 (lowest creation_time)

AVG WAIT TIME: 6.75
AVG RUN TIME: 1.625

CFS SCHEDULER:

PID	State	vruntime	Nice	Weight
-------------------------------------------
1	sleep 	15	18	1586
2	sleep 	11	18	1586
3	sleep 	2	13	4904
4	runble	1	15	3121
5	run   	0	-9	7214889101763438966
6	runble	0	7	18705
7	runble	0	-10	32776920117747823
8	runble	0	-1	0
9	runble	0	-8	7812726566314275689
10	runble	0	-14	12320
11	runble	0	9	11916
--> Scheduling PID 5 (lowest vRuntime)

AVG WAIT TIME: ~2.5
AVG RUN TIME: 3.72

RR SCHEDULER:

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

AVG WAIT TIME: ~ 0
AVG RUN TIME: 13.6


