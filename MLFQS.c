
#include <string.h>
#include <stdlib.h>
#include "prioque.h"

// ANTHONY ALVAREZ (89-9962639)
// MLFQS.C will simulate a four-level multi-level feedback queue scheduler.
// Each queue in the scheduler should use a Round Robin schedule.

// QUANTUM, GOOD, AND BAD LAYOUT FOR ALL LEVELS
// 
// First level,  "q" = 10 | "b" = 1  | "g" = inf 	(Round Robin) 
// Second level, "q" = 30 | "b" = 2  | "g" = 1 		(Round Robin)
// Third level,  "q" = 100| "b" = 2  | "g" = 2		(Round Robin)
// Fourth level, "q" = 200| "b" = inf| "g" = 2		(Round Robin)

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

// This Process Struct will represent a "process" in the CPU Scheduler.
// The struct will keep track of its Burst, IO, repeat, and more while
// also keeping track of the limits of each g, b, and quantum based on
// the level it exists in.
typedef struct Process {
	unsigned long arrivalTime;		// When process should be inserted into scheduler.
	unsigned long PID; 				// Process Identification Number
	unsigned long burst;			// The amount of CPU it wants to use in a specific run phase.
	unsigned long IO;				// The amount of IO it wants to do in a specific IO phase.
	unsigned long repeat;			// The amount of times it repeats [RUN -> IO] phases.
	unsigned long burstRemaining;	// The amount of burst remaining till it needs to go to IO Phase.
	unsigned long IORemaining;		// The amount of IO remaining till it ticks down repeat.
	unsigned long quantum;			// The maximum quantum based on the queue the process is in.
	unsigned long quantumRemaining;	// The amount of quantum remaining till its considered a bad behavior.
	unsigned long usageCPU;			// Used to track the usage of CPU. 
	int inWhichQueue;				// Keeps track of which Queue the process is in.
	int b; 							// Demotion counter.
	int g; 							// Promotion counter.
	int bLim;						// Max "b" till demotion
	int gLim;						// Max "g" till promotion 
	struct Process * nextSet;		// Further Explanation below..
} Process;

// Further Explanation on nextSet:
//
// The process pointer allows for duplicate processes to be
// part of a linked list. When multiple behaviors with the
// same PID arrive, they are linked to each other and when
// one behavior finished it rewrites the first element with
// the second elements behavior and points to the third one.
//
// Queue level1: [P1, P2, P3, P4, ....]
//						  ^^
//						  P3
//						  ^^
//						  P3

// FUTURE CORRECTIONS:
// 
// This might make me lost points but... my IO is incorrect.
// Processes are kicked out of their queue when its IO time.
// They are supposed to keep their spot and then return but
// I found out too late so my structure is incorrect.
//
// TODO: Processes keep their spot when they reach IO phase instead of getting kicked off.
// TODO: After grading, change output to be more visually clear.
// TODO: Create more methods for tasks that i've copied a million times.

// ALL FUNCTION DECLARATIONS
// go to actual methods for better explanation of use.
int processesExist();
int readyProcessExists(Queue*,Queue*,Queue*,Queue*);
int grabReadyProcess(Queue*, Queue*, Queue*, Queue*, Process*);
void init_all_queues();

// ALL GLOBAL VARIABLES OR STRUCTS
Queue notArrived;
Queue blocked;
Queue level1;
Queue level2;
Queue level3;
Queue level4;
Queue terminated;
unsigned long element;

int main(int argc, char *argv[]) {

	// Initializing Queues.
	init_all_queues();

	// Section 1.1:
	// While loop that gathers all input, turns it into processes and puts them in queue.
	element=1;
	Process newP;
	Process *prevP;
	while (scanf("%lu %lu %lu %lu %lu", &(newP.arrivalTime), &(newP.PID), &(newP.burst), &(newP.IO), &(newP.repeat)) == 5) {
		newP.burstRemaining = newP.burst;
		newP.IORemaining = newP.IO;
		newP.nextSet = NULL;
		if(!empty_queue(&notArrived)) {
			if(newP.PID == prevP->PID) {
				Process* child = (Process *) malloc(sizeof(Process));
				child->burst = newP.burst;
				child->burstRemaining = newP.burstRemaining;
				child->IO = newP.IO;
				child->IORemaining = newP.IORemaining;
				child->PID = newP.PID;
				child->repeat = newP.repeat;
				child->arrivalTime = newP.arrivalTime;
				prevP->nextSet = child;
				prevP = child;
			}
			
			else {
				add_to_queue(&notArrived, &newP, 0);
				while(!end_of_queue(&notArrived)) {
					prevP = (Process *) pointer_to_current(&notArrived);
					next_element(&notArrived);
				}
				element++;
			}
			
		}
		else {
			add_to_queue(&notArrived, &newP, 0);
			prevP = (Process *) pointer_to_current(&notArrived);
			element++;
		}
	}

	free(prevP);
	prevP = NULL;
	
	

	Process nullProc = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	Process procExecuting = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	unsigned long clock=0; // Internal Scheduler Clock
	while(processesExist(&notArrived,&blocked,&level1,&level2,&level3,&level4)) {

		// Section 2.1: Handling arrival processes.
		// I'm using Process "procArriving" to keep track of processes that are currently arriving
		// from the "notArrived" array.
		Process procArriving;
		rewind_queue(&notArrived);
		peek_at_current(&notArrived, &procArriving, 0);
		
		// This while loop will enqueue all processes that arrive at a certain clock time
		// into the level1 queue.
		while(procArriving.arrivalTime == clock) {
			printf("PID: %lu, ARRIVAL TIME: %lu\n",
			procArriving.PID, procArriving.arrivalTime);
			printf("CREATE: Process %lu entered the ready queue at time %lu.\n", 
					procArriving.PID, clock);
			delete_current(&notArrived);
			procArriving.quantum = 10;
			procArriving.quantumRemaining = 10;
			procArriving.inWhichQueue = 1;
			procArriving.gLim = -1;
			procArriving.bLim = 1;
			procArriving.g = 0;
			procArriving.b = 0;
			add_to_queue(&level1, &procArriving, 0);
			element++;

			if(!peek_at_current(&notArrived, &procArriving, 0))
				break;
			

		}

		// Section 2.2: Handles choosing process to execute.
		// If empty, null process will tick.
		if(!readyProcessExists(&level1,&level2,&level3,&level4)) {
			nullProc.usageCPU = nullProc.usageCPU + 1;
		}
		// If no process running but there exists some ready
		// processes, then set highest process as running.
		else if(procExecuting.PID == 0) {
				grabReadyProcess(&level1,&level2,&level3,&level4,&procExecuting);
				printf("RUN: Process %lu started execution from level %d at time %lu; ",
				procExecuting.PID, procExecuting.inWhichQueue, clock);
				printf("wants to execute for %lu ticks.\n", 
				procExecuting.burstRemaining);
		}
		// If a process is currently running, then begin execution
		else {
			// tick burst and quantum.
			procExecuting.burstRemaining--;
			procExecuting.quantumRemaining--;
			procExecuting.usageCPU++;
			
			// If burst is 0, check if IO needs to be done or process is finished
			if(procExecuting.burstRemaining == 0) {
				// If process had IO but no burst, then send to IO buffer to tick
				// and set processExecuting to nullProc.
				if(procExecuting.IORemaining > 0) {

					if(procExecuting.quantumRemaining == 0) {procExecuting.b++;}
					else {
						if(procExecuting.b == 0) {procExecuting.g++;}
						else {procExecuting.b = 0;}
					}
					procExecuting.quantumRemaining = procExecuting.quantum;
					printf("I/O: Process %lu blocked for I/O at time %lu.\n", 
						procExecuting.PID, clock);
					add_to_queue(&blocked, &procExecuting, 0);
					switch(procExecuting.inWhichQueue) {
						case 1:
							delete_current(&level1);
							break;
						case 2:
							delete_current(&level2);
							break;
						case 3:
							delete_current(&level3);
							break;
						case 4:
							delete_current(&level4);
							break;
						default:
							printf("ERROR: process is lost\n");
							exit(0);
							break;
					}
					procExecuting = nullProc;
				}
				// If no burst and IO, then process is finished and must be terminated.
				else {
					printf("FINISHED: Process %lu finished at time %lu.\n",
						procExecuting.PID, clock);
					add_to_queue(&terminated, &procExecuting, 0);
					switch(procExecuting.inWhichQueue) {
						case 1:
							delete_current(&level1);
							break;
						case 2:
							delete_current(&level2);
							break;
						case 3:
							delete_current(&level3);
							break;
						case 4:
							delete_current(&level4);
							break;
						default:
							printf("ERROR: process is lost\n");
							exit(0);
							break;
					}
					procExecuting = nullProc;
				}

			}
			// If burst is not 0, check if quantum was met
			else if(procExecuting.quantumRemaining == 0) {
				procExecuting.b++;
				procExecuting.g = 0;

				// Demotion checking, if b = bLim (b's limit for queue level) then demote.
				if(procExecuting.b == procExecuting.bLim) {
					procExecuting.inWhichQueue++;
					printf("QUEUED: Process %lu queued at level %d at time %lu.\n",
					procExecuting.PID, procExecuting.inWhichQueue, clock);
					// In demotion & promotion, I set the b, g, and quantum requirements.
					switch(procExecuting.inWhichQueue) {
						case 2:
							procExecuting.b = 0;
							procExecuting.bLim = 2;
							procExecuting.gLim = 1;
							procExecuting.quantum = 30;
							procExecuting.quantumRemaining = 30;
							add_to_queue(&level2, &procExecuting, 0);
							delete_current(&level1);
							break;
						case 3:
							procExecuting.b = 0;
							procExecuting.bLim = 2;
							procExecuting.gLim = 2;
							procExecuting.quantum = 100;
							procExecuting.quantumRemaining = 100;
							add_to_queue(&level3, &procExecuting, 0);
							delete_current(&level2);
							break;
						case 4:
							procExecuting.b = 0;
							procExecuting.bLim = -1;
							procExecuting.gLim = 2;
							procExecuting.quantum = 200;
							procExecuting.quantumRemaining = 200;
							add_to_queue(&level4, &procExecuting, 0);
							delete_current(&level3);
							break;
						default:
							printf("ERROR: Process is lost.\n");
							exit(0);
							break;
					}
					procExecuting = nullProc;
				}
				else {
					// If b was counted up but it is not enough to demote, then we put
					// the current executing process at the back of the current queue.
					procExecuting.quantumRemaining = procExecuting.quantum;
					printf("QUEUED: Process %lu queued at level %d at time %lu.\n",
					procExecuting.PID, procExecuting.inWhichQueue, clock);
					switch(procExecuting.inWhichQueue) {
						case 1:
							rewind_queue(&level1);
							add_to_queue(&level1, &procExecuting, 0);
							delete_current(&level1);
							break;
						case 2:
							rewind_queue(&level2);
							add_to_queue(&level2, &procExecuting, 0);
							delete_current(&level2);
							break;
						case 3:
							rewind_queue(&level3);
							add_to_queue(&level3, &procExecuting, 0);
							delete_current(&level3);
							break;
						case 4:
							rewind_queue(&level4);
							add_to_queue(&level4, &procExecuting, 0);
							delete_current(&level4);
							break;
						default:
							printf("ERROR: Process is lost.\n");
							exit(0);
							break;
					}
					procExecuting = nullProc;
				}
			}
			// Promotion Checking, if proc.g = proc.gLim then we move the proc
			// up a level and set all proc.variables needed to match higher level
			// requirements. (gLim is the amount of g needed for promotion)

			// Needs to be deleted.
			else if(procExecuting.g == procExecuting.gLim) {
				
				procExecuting.inWhichQueue--;
				printf("QUEUED: Process %lu queued at level %d at time %lu.\n",
				procExecuting.PID, procExecuting.inWhichQueue, clock);
				
				switch(procExecuting.inWhichQueue) {
					case 1:
						procExecuting.b = 0;
						procExecuting.g = 0;
						procExecuting.bLim = 1;
						procExecuting.gLim = -1;
						procExecuting.quantum = 10;
						procExecuting.quantumRemaining = 10;
						add_to_queue(&level1, &procExecuting, 0);
						delete_current(&level2);
						break;
					case 2:
						procExecuting.b = 0;
						procExecuting.g = 0;
						procExecuting.bLim = 2;
						procExecuting.gLim = 1;
						procExecuting.quantum = 30;
						procExecuting.quantumRemaining = 30;
						add_to_queue(&level2, &procExecuting, 0);
						delete_current(&level3);
						break;
					case 3:
						procExecuting.b = 0;
						procExecuting.g = 0;
						procExecuting.bLim = 2;
						procExecuting.gLim = 2;
						procExecuting.quantum = 100;
						procExecuting.quantumRemaining = 100;
						add_to_queue(&level3, &procExecuting, 0);
						delete_current(&level4);
						break;
					default:
						printf("ERROR: Process is lost.\n");
						exit(0);
						break;
				}

				procExecuting = nullProc;

			}

			// If no process is in running state after execution actions, 
			// check if there exists ready process and set it as executing process.
			if(procExecuting.PID == 0) {

				if(readyProcessExists(&level1,&level2,&level3,&level4)) {

					grabReadyProcess(&level1,&level2,&level3,&level4,&procExecuting);
					printf("RUN: Process %lu started execution from level %d at time %lu; ",
					procExecuting.PID, procExecuting.inWhichQueue, clock);
					printf("wants to execute for %lu ticks.\n",
					procExecuting.burstRemaining);

				}

			}
			// If process is currently executing, check if there exists a higher
			// level process and if so, set the higher level process as currently
			// running.
			else {

				Process temp;
				grabReadyProcess(&level1,&level2,&level3,&level4,&temp);

				if(procExecuting.inWhichQueue > temp.inWhichQueue) {

					printf("QUEUED: Process %lu queued at level %d at time %lu.\n",
					procExecuting.PID, procExecuting.inWhichQueue, clock);
					Process *adjustments = NULL;
					switch(procExecuting.inWhichQueue) {
						case 1:
							adjustments = pointer_to_current(&level1);
							adjustments->burstRemaining = procExecuting.burstRemaining;
							adjustments->b = procExecuting.b;
							adjustments->g = procExecuting.g;
							adjustments->quantumRemaining = procExecuting.quantumRemaining;
							adjustments->usageCPU = procExecuting.usageCPU;
							break;
						case 2:
							adjustments = pointer_to_current(&level2);
							adjustments->burstRemaining = procExecuting.burstRemaining;
							adjustments->b = procExecuting.b;
							adjustments->g = procExecuting.g;
							adjustments->quantumRemaining = procExecuting.quantumRemaining;
							adjustments->usageCPU = procExecuting.usageCPU;
							break;
						case 3:
							adjustments = pointer_to_current(&level3);
							adjustments->burstRemaining = procExecuting.burstRemaining;
							adjustments->b = procExecuting.b;
							adjustments->g = procExecuting.g;
							adjustments->quantumRemaining = procExecuting.quantumRemaining;
							adjustments->usageCPU = procExecuting.usageCPU;
							break;
						case 4:
							adjustments = pointer_to_current(&level4);
							adjustments->burstRemaining = procExecuting.burstRemaining;
							adjustments->b = procExecuting.b;
							adjustments->g = procExecuting.g;
							adjustments->quantumRemaining = procExecuting.quantumRemaining;
							adjustments->usageCPU = procExecuting.usageCPU;
							break;
						default:
							printf("ERROR: process is lost.\n");
							exit(0);
							break;
					}

					grabReadyProcess(&level1,&level2,&level3,&level4,&procExecuting);

					printf("RUN: Process %lu started execution from level %d at time %lu; ",
					procExecuting.PID, procExecuting.inWhichQueue, clock);
					printf("wants to execute for %lu ticks.\n",
					procExecuting.burstRemaining);
				}

			}
				
		}
		

		// Section 2.3: handles IO / Promotion / Demotion
		
		// FOR I/O, all processes in the blocked queue needs to get one tick of their I/O completed.
		// Promotion checking will be in IO instead.
		if(!empty_queue(&blocked)) {

			rewind_queue(&blocked);
			while(!end_of_queue(&blocked)) {

				Process *curr = (Process *) pointer_to_current(&blocked);
				curr->IORemaining--;
				
				// If a process has no remaining IO, return them to their queue.
				if(curr->IORemaining == 0) {
					
					curr->repeat--;
					if(curr->repeat == 0) {

						if(curr->nextSet != NULL && curr->nextSet->PID == curr->PID) {
							// IF error, fix with malloc.
							Process *next = curr->nextSet;
							curr->burst = next->burst;
							curr->burstRemaining = next->burstRemaining;
							curr->IO = next->IO;
							curr->IORemaining = next->IORemaining;
							curr->repeat = next->repeat;
							if(next->nextSet != NULL) {
								next = next->nextSet;
								curr->nextSet = next;
							}
							else {
								curr->nextSet = NULL;
							}

						}
						else {
							curr->burstRemaining = curr->burst;
						}
					}
					else {
						curr->burstRemaining = curr->burst;
						curr->IORemaining = curr->IO;
					}

					switch(curr->inWhichQueue) {

						case 1:
							// Demotion of process from Level 1 -> 2
							// Put at rear.
							if(curr->b == curr->bLim) {
								curr->inWhichQueue++;
								curr->b = 0;
								curr->bLim = 2;
								curr->g = 0;
								curr->gLim = 1;
								curr->quantum = 30;
								curr->quantumRemaining = 30;
								add_to_queue(&level2, curr, 0);
							}
							// Process returns to level 1 at the rear.
							else {
								add_to_queue(&level1, curr, 0);
							}
							break;

						case 2:
							// Promotion of process from Level 2 -> 1
							// Put at rear.
							if(curr->g == curr->gLim) {
								curr->inWhichQueue--;
								curr->g = 0;
								curr->gLim = -1;
								curr->b = 0;
								curr->bLim = 1;
								curr->quantum = 10;
								curr->quantumRemaining = 10;
								add_to_queue(&level1, curr, 0);
							}
							// Demotion of process from Level 2 -> 3
							// Put at rear.
							else if(curr->b == curr->bLim) {
								curr->inWhichQueue++;
								curr->b = 0;
								curr->bLim = 2;
								curr->g = 0;
								curr->gLim = 2;
								curr->quantum = 100;
								curr->quantumRemaining = 100;
								add_to_queue(&level3, curr, 0);
							}
							// Process returns to level 2 at the rear.
							else {
								add_to_queue(&level2, curr, 0);
							}
							break;

						case 3:
							// Promotion of process from Level 3 -> 2
							// Put at rear.
							if(curr->g == curr->gLim) {
								curr->inWhichQueue--;
								curr->g = 0;
								curr->gLim = 1;
								curr->b = 0;
								curr->bLim = 2;
								curr->quantum = 30;
								curr->quantumRemaining = 30;
								add_to_queue(&level2, curr, 0);
							}
							// Demotion of process form Level 3 -> 4
							// Put at rear.
							else if(curr->b == curr->bLim) {
								curr->inWhichQueue++;
								curr->b = 0;
								curr->bLim = -1;
								curr->g = 0;
								curr->gLim = 2;
								curr->quantum = 200;
								curr->quantumRemaining = 200;
								add_to_queue(&level4, curr, 0);
							}
							// Process returns to level 3 at the rear.
							else {
								add_to_queue(&level3, curr, 0);
							}
							break;

						case 4:
							// Promotion of process from Level 4 -> 3
							// Put at rear.
							if(curr->g == curr->gLim) {
								curr->inWhichQueue--;
								curr->g = 0;
								curr->gLim = 2;
								curr->b = 0;
								curr->bLim = 2;
								curr->quantum = 100;
								curr->quantumRemaining = 100;
								add_to_queue(&level3, curr, 0);
							}
							// Process returns to level 4 at the rear.
							else {
								add_to_queue(&level4, curr, 0);
							}
							break;

						default:
							// If process isn't in Q 1->4, they are lost.
							printf("ERROR: lost track of where process belonged");
							exit(0);
							break;

					}
					
					delete_current(&blocked);
				}
				
				// This makes sure that we dont move forward
				// if a process being removed earlier was on
				// the edge of the blocked queue.
				if(!end_of_queue(&blocked)) {
					next_element(&blocked);
				}
			}
		}

		// If the last process finished its execution, close the scheduler, we are done!
		if(!processesExist(&notArrived,&blocked,&level1,&level2,&level3,&level4)) {
			break;
		}

		clock++;
	}

	printf("\nScheduler shutdown at time %lu.\n", clock);
	
	

	// WILL BE REMOVED JUST CHECKING IF ALL THE PROCESSES REACHED THE TERMINATED QUEUE/STATE
	// SHOULD CONTAIN ALL PROCESSES
	printf("\n[*] Printing Terminated Queue...\n");
	element=1;
	rewind_queue(&terminated);
	while(!end_of_queue(&terminated)) {
		Process *curr = (Process *) pointer_to_current(&terminated);
		printf("Element: %lu PID: %lu Time: %lu Burst: %lu I/O: %lu Repeat: %lu\n",
				element++,
				curr->PID,
				curr->arrivalTime,
				curr->burstRemaining,
				curr->IORemaining,
				curr->repeat);
		next_element(&terminated);
	}

	printf("\n[*] Checking NULL process usage ...\n");
	printf("NULL Process' CPU Usage: %lu\n", nullProc.usageCPU); 
	printf("\n[*] Exiting program...\n");

}

// init_all_queues() will initialize all the queues that we will
// be using for the scheduler.
void init_all_queues() {
	init_queue(&notArrived, sizeof(Process), TRUE, FALSE, FALSE);
	init_queue(&blocked, sizeof(Process), TRUE, FALSE, FALSE);
	init_queue(&level1, sizeof(Process), TRUE, FALSE, FALSE);
	init_queue(&level2, sizeof(Process), TRUE, FALSE, FALSE);
	init_queue(&level3, sizeof(Process), TRUE, FALSE, FALSE);
	init_queue(&level4, sizeof(Process), TRUE, FALSE, FALSE);
	init_queue(&terminated, sizeof(Process), TRUE, FALSE, FALSE);
}

// processesExist() checks if atleast one process exists in
// all queues that the scheduler uses. This makes sure the scheduler
// doesn't close early because of not seeing a future arrival or blocked process.
// Returns 1 if TRUE, 0 if FALSE.
int processesExist() {
	
	if(!empty_queue(&notArrived) || !empty_queue(&blocked) || !empty_queue(&level1) || !empty_queue(&level2) || !empty_queue(&level3) || !empty_queue(&level4)) {
		return 1;
	}
	else {
		return 0;
	}
}

// readyProcessExists() checks if level 1-4 queues contain
// any processes that are ready for execution. 
// Returns 1 if TRUE, 0 if FALSE. 
int readyProcessExists(Queue *one, Queue *two, Queue *three, Queue *four) {
	
	if(!empty_queue(one) || !empty_queue(two) || !empty_queue(three) || !empty_queue(four)) {
		return 1;
	}
	else {
		return 0;
	}

}

// grabReadyProcess() grabs the highest level process that
// is currently ready. It copies the data from the ready
// process to Process *proc.
// Returns 1 if it grabbed a ready process, 0 if it did not.
int grabReadyProcess(Queue *one, Queue *two, Queue *three, Queue *four, Process *proc) {

	int ret = 0;
	if(pointer_to_current(one) != NULL) {
		rewind_queue(one);
		peek_at_current(one, proc, 0);
		ret = 1;
		return ret;
	}
	else if(pointer_to_current(two) != NULL) {		
		rewind_queue(two);
		peek_at_current(two, proc, 0);
		ret = 1;
		return ret;
	}
	else if(pointer_to_current(three) != NULL) {
		rewind_queue(three);
		peek_at_current(three, proc, 0);
		ret = 1;
		return ret;
	}
	else if(pointer_to_current(four) != NULL) {
		rewind_queue(four);
		peek_at_current(four, proc, 0);
		ret = 1;
		return ret;
	}
	else {
		return ret;
	}
}

