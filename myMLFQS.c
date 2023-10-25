
#include <string.h>
#include "prioque.h"

// ANTHONY ALVAREZ (89-9962639)
// myMLFQS will simulate a four-level multi-level feedback queue scheduler.
// Each queue in the scheduler should use a Round Robin schedule.

// QUANTUM LAYOUT FOR LEVELS:
// 
// First level, low quantum for I/O-bound processes. 	(Round Robin) 
// Second level, medium quantum. 			(Round Robin)
// Third level, higher quantum. 			(Round Robin)
// Fourth level, highest quantum.			(Round Robin)

// FOUR-LEVEL PRIORTY SCHEME:
// 
// The four queue levels should follow a strict priority scheme.
// For example, for a process in the second level to execute, the
// first level queue should be empty. When a process arrives in
// an upper-level queue while a process is executing in a lower
// level queue, the lower priority process is preempted and
// remains in place in the same queue until it gets to execute again.

// RULES FOR CHOOSING WHICH PROCESS EXECUTES:
// 
// 1. New processes are placed at the end of the highes priority queue.
// 2. If Priority(A) > Priority(B), A is selected for execution.
// 3. If Priority(A) = Priority(B), use RR to schedule A and B.
// 4. If a higher priority process arrives, the currently executing
//    process is preempted after the current clock tick.
//    (but stays in place in its current queue)
// 5. (DEMOTION) If a process uses its entire quantum at a particular
//    priority "b" times, its priority is reduced and it moves down one level. 
// 6. (PROMOTION) If a process doesn't use its entire quantum at a particular
//    priority "g" times, its priority is increased and it moves up one level.

// NOTES ON RESETTING "b" (DEMOTION) AND "g" (PROMOTION):
// 
// For demotion calculations, doing an I/O resets "b". The idea here is that
// burning through the entire quantum "b" times in a row before you do I/O
// means the process should be demoted. If the process does I/O before using
// the entire quantum, "b" is reset.
//
// For promotion calculations, "g" can't be reset on each I/O, because the idea
// is that the process gets its full CPU need, does I/O, returns to ready queue,
// gets its full CPU need, etc., without exhausting the quantum, "g" times in a
// row, to be promoted. So the "g" can't be reset when the process does an I/O,
// or you can't track this.

// "q", "b", "g" of each level
//
// Level 1 Ultra High:	"q" = 10  | "b" = 1   | "g" = inf
// Level 2 High:	"q" = 30  | "b" = 2   | "g" = 1
// Level 3 Medium: 	"q" = 100 | "b" = 2   | "g" = 2
// Level 4 Lowest:	"q" = 200 | "b" = inf | "g" = 2

// For a visual of the MLFQS look at the first diagram given in the Assignment #2 PDF

// THE NULL PROCESS SITUATION:
// When the scheduler begins, the time should be 0. Whenever there is no process
// to schedule (all processes are doing I/O or no processes exist), a special
// process called the "null" process should execute. The scheduler should continue
// to increment its clock during the execution of the null process, waiting
// for another process to become ready. Your scheduler should exit when all processes
// from the input have been executed completely.

#define VOID -1
#define RUNNING 0
#define BLOCKED 1    
#define READY 2
#define TERMINATED 3
#define CREATED 4  

typedef struct Process {
	unsigned long arrivalTime;	// 1st input
	unsigned long PID; 		// 2nd input
	int priority;
	unsigned long burst;		// 3rd input
	unsigned long IO;		// 4th input
	unsigned long repeat;		// 5th input
	unsigned long burstRemaining;
	unsigned long IORemaining;
	unsigned long quantum;
	unsigned long usageCPU;
	int state;
	int g; // Promotion
	int b; // Demotion
} Process;

int Process_Compare(const void *e1, const void *e2) {
	Process *p1 = (Process *) e1;
	Process *p2 = (Process *) e2;
	if(p1->priority == p2->priority) {
		return 1;
	}
	else {
		return 0;
	}
}

int main(int argc, char *argv[]) {

	// SECTION 1: THIS SECTION IS INPUT READING (1.1) AND TESTING (1.2) IF THE PROCESSES WERE READ.
	// NOTE: test needs to be removed by turn in date.	

	// Initializing Queues.
	Queue notArrived;
	unsigned long element;
	init_queue(&notArrived, sizeof(Process), TRUE, Process_Compare, FALSE);  

	// Section 1.1:
	// While loop that gathers all input, turns it into processes and puts them in queue.
	// This needs to be fixed, try using the strsep() from Strings.c in scantrons.
	printf("[*] Expecting input: \n");
	element=1;
	Process newP;
	while (scanf("%lu %lu %lu %lu %lu", &(newP.arrivalTime), &(newP.PID), &(newP.burst), &(newP.IO), &(newP.repeat)) == 5) {
		printf("Process #%lu with PID: %lu added with to the queue.\n", element, newP.PID);
		newP.priority = 0;//newP.burst; use this for setting priority based on burst times
		newP.burstRemaining = newP.burst;
		add_to_queue(&notArrived, &newP, newP.priority);
		element++;
	}

	// Section 1.2:
	// Incorporate a while loop that prints all processes from queue (Just for testing must be removed by turn in).
	element=0;
	rewind_queue(&notArrived);
	printf("\n");
	printf("[*] Queue contains:\n");
	while (!end_of_queue(&notArrived)) {
		printf("Element: %lu PID: %lu Time: %lu Priority: %d\n",
				++element, 
				((Process *) pointer_to_current(&notArrived))->PID,
				((Process *) pointer_to_current(&notArrived))->arrivalTime,
				current_priority(&notArrived));
		next_element(&notArrived);
	}

	printf("\n");

	
	// END OF SECTION 1
	
	// SECTION 2: CREATING THE SCHEDULERS HIGH-LEVEL STRUCTURE (basic attempt)
	// 2.1: HANDLES ARRIVALS | 2.2: HANDLES EXECUTING | 2.3 HANDLES IO/PROMOTION/DEMOTION/EXIT (NOT DONE) 
	
	printf("[*] Entering High Level Queue....\n");

	Queue blocked;
	Queue level1;
	init_queue(&blocked, sizeof(Process), TRUE, Process_Compare, FALSE);
	init_queue(&level1, sizeof(Process), TRUE, Process_Compare, FALSE);
	Process nullProc = {0,0,0,0,0,0,0,0,0,0,READY};
	Process procExecuting = {0,0,0,0,0,0,0,0,0,0,VOID};
	int pExecuting;
	unsigned long clock=0; // Internal Scheduler Clock
	while(!empty_queue(&notArrived) || !empty_queue(&level1)) {

		// Section 2.1: Handling arrival processes.
		// I'm using Process "procArriving" to keep track of processes that are currently arriving
		// from the "notArrived" array.
		Process procArriving;
		int pArrival;
		rewind_queue(&notArrived);
		peek_at_current(&notArrived, &procArriving, &pArrival);
		
		// This while loop will enqueue all processes that arrive at a certain clock time
		// into the level1 queue.
		while(procArriving.arrivalTime == clock) {
			printf("CREATE: Process %lu entered the ready queue at time %lu.\n", 
					procArriving.PID, clock);
			delete_current(&notArrived);
			procArriving.state = READY;
			procArriving.quantum = 10;
			add_to_queue(&level1, &procArriving, pArrival);
			element++;

			if(!peek_at_current(&notArrived, &procArriving, &pArrival))
				break;
		}

		// Section 2.2: Handles choosing process to execute.
		// If empty, null process will tick.
		if(empty_queue(&level1)) {
			nullProc.usageCPU = nullProc.usageCPU + 1;
		}
		else {
			// If nothing is executing, grab process in front and begin execution.
			if(procExecuting.state == VOID) {
				rewind_queue(&level1);
				peek_at_current(&level1, &procExecuting, &pExecuting);
				printf("RUN: Process %lu started execution from level 1 at time %lu;\n",
						procExecuting.PID, clock);
				printf("wants to execute for %lu ticks.\n", procExecuting.burstRemaining);
				
				// trickle down burst one tick at a time
				if(procExecuting.burstRemaining > 0) {
					procExecuting.burstRemaining = procExecuting.burstRemaining - 1;
					// procExecuting.usageCPU++ or something... incorporate usage tracking.
				}
				// burst must be depleted, check if i/o needs to be done, if not then tick down repeat.
				else {
					if(procExecuting.IORemaining > 0) {
						// I/O needs to be done, remove processes from level1 queue
						// and move them into the blocked queue.
						printf("FINISHED: Process %lu finished at time %lu.\n", 
								procExecuting.PID, clock);		
						procExecuting.state = BLOCKED;
						add_to_queue(&blocked, &procExecuting, procExecuting.priority);
						delete_current(&level1);
						procExecuting.state = VOID;
						
						
					}
					// No burst or IO, so check if repeat needs to be done.
					// WONT BE REACHED BECAUSE ITS BURST->IO->REPEAT BUT WE DELETE AT IO FOR TESTING
					else {
						if(procExecuting.repeat > 0) {
							procExecuting.repeat = procExecuting.repeat - 1;
							// Need to restart burst and IO but for testing, it remains untouched.
						}

						else {
							// no burst, io, or repeat left so terminate the process.
							delete_current(&level1);
							procExecuting.state = VOID;
						}
					}
				}
			}
			// If something is executing, then check if a higher priority process exists. (NOT IMPLEMENTED YET)
			// If not, then just tick down burst or IO or repeat.
			else {
				// Burst
				if(procExecuting.burstRemaining > 0) {
					procExecuting.burstRemaining = procExecuting.burstRemaining - 1;
				}

				else {
					// IO (DELETING PROCESSES AFTER ONE SET OF BURST FOR NOW)
					if(procExecuting.IORemaining > 0) {
						rewind_queue(&level1);
						printf("FINISHED: Process %lu finished at time %lu.\n", 
								procExecuting.PID, clock);
						procExecuting.state = BLOCKED;
						add_to_queue(&blocked, &procExecuting, procExecuting.priority);
						delete_current(&level1);
						procExecuting.state = VOID;
					}
					// No burst or IO, so check repeat
					// (WONT BE REACHED FOR NOW)
					else {
						if(procExecuting.repeat > 0) {
							procExecuting.repeat = procExecuting.repeat - 1;
							// need to reset io and burst, leave for later.
						}
						
						else {
							// no burst, io, or repeat left so its terminated.
							delete_current(&level1);
							procExecuting.state = VOID;
						}	
					}	
				}
			}
		}

		// Section 2.3: handles Exit/IO/Promotion/Demotion
		// START HERE
		
		// FOR I/O, all processes in the blocked queue needs to get one tick of their I/O completed.
		
		clock++;
	}

	printf("\n[*] Scheduler shutdown at time %lu.\n", clock);
	
	// END OF SECTION 2

	// WILL BE REMOVED JUST CHECKING IF ANY PROCESSES ARE PASSING THROUGH.
	// THERE SHOULD BE NOTHING IN THE LEVEL 1 QUEUE.
	printf("\n[*] Printing Level 1 Queue... (should be nothing below)\n");
	element=1;
	rewind_queue(&level1);
	while (!end_of_queue(&level1)) {
		printf("Element: %lu PID: %lu Time: %lu Priority: %d\n",
				element++,
				((Process *) pointer_to_current(&level1))->PID,
				((Process *) pointer_to_current(&level1))->arrivalTime,
				current_priority(&level1));
		next_element(&level1);
	}
	
	// WILL BE REMOVED JUST CHECKING IF ALL THE PROCESSES REACHED THE BLOCK STATE.
	// SHOULD CONTAIN ALL PROCESSES.
	printf("\n[*] Printing Blocked Queue... (should be all the processes)\n");
	element=1;
	rewind_queue(&blocked);
	while (!end_of_queue(&blocked)) {
		printf("Element: %lu PID: %lu Time: %lu Priority: %d\n",
				element++,
				((Process *) pointer_to_current(&blocked))->PID,
				((Process *) pointer_to_current(&blocked))->arrivalTime,
				current_priority(&blocked));
		next_element(&blocked);
	}

	printf("\n");

	printf("[*] Checking NULL process usage ...\n");
	printf("NULL Process' CPU Usage: %lu\n", nullProc.usageCPU); 
	printf("\n[*] Exiting program...\n");

}
