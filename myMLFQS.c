
#include <string.h>
#include <stdlib.h>
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
	unsigned long quantumRemaining;
	unsigned long usageCPU;
	int inWhichQueue;
	int state;
	int b; // Demotion
	int g; // Promotion
} Process;

int processesExist(Queue*,Queue*,Queue*,Queue*,Queue*,Queue*);
int readyProcess(Queue*,Queue*,Queue*,Queue*);
int grabReadyProcess(Queue*, Queue*, Queue*, Queue*, Process*);
int demotionCheck(Queue*,Queue*,Queue*,Queue*,Process*,Queue*,Queue*); 

int main(int argc, char *argv[]) {

	// SECTION 1: THIS SECTION IS INPUT READING (1.1) AND TESTING (1.2) IF THE PROCESSES WERE READ.
	// NOTE: test needs to be removed by turn in date.	

	// Initializing Queues.
	Queue notArrived;
	unsigned long element;
	init_queue(&notArrived, sizeof(Process), TRUE, FALSE, FALSE);  

	// Section 1.1:
	// While loop that gathers all input, turns it into processes and puts them in queue.
	// This needs to be fixed, try using the strsep() from Strings.c in scantrons.
	printf("[*] Expecting input: \n");
	element=1;
	Process newP;
	while (scanf("%lu %lu %lu %lu %lu", &(newP.arrivalTime), &(newP.PID), &(newP.burst), &(newP.IO), &(newP.repeat)) == 5) {
		printf("Process #%lu with PID: %lu added with to the queue.\n", element, newP.PID);
		//newP.burst; use this for setting priority based on burst times
		newP.burstRemaining = newP.burst;
		newP.IORemaining = newP.IO;
		add_to_queue(&notArrived, &newP, 0);
		element++;
	}

	// Section 1.2:
	// Incorporate a while loop that prints all processes from queue (Just for testing must be removed by turn in).
	element=0;
	rewind_queue(&notArrived);
	printf("\n");
	printf("[*] Queue contains:\n");
	while (!end_of_queue(&notArrived)) {
		printf("Element: %lu PID: %lu Time: %lu\n",
				++element, 
				((Process *) pointer_to_current(&notArrived))->PID,
				((Process *) pointer_to_current(&notArrived))->arrivalTime);
		next_element(&notArrived);
	}

	printf("\n");

	
	// END OF SECTION 1
	
	// SECTION 2: CREATING THE SCHEDULERS HIGH-LEVEL STRUCTURE (basic attempt)
	// 2.1: HANDLES ARRIVALS | 2.2: HANDLES EXECUTING | 2.3 HANDLES IO/PROMOTION/DEMOTION/EXIT (NOT DONE) 
	// Change all peek_at_current to pointer_to_current	
	printf("[*] Entering High Level Queue....\n");

	Queue blocked;
	Queue level1;
	Queue level2;
	Queue level3;
	Queue level4;
	Queue terminated;
	init_queue(&blocked, sizeof(Process), TRUE, FALSE, FALSE);
	init_queue(&level1, sizeof(Process), TRUE, FALSE, FALSE);
	init_queue(&level2, sizeof(Process), TRUE, FALSE, FALSE);
	init_queue(&level3, sizeof(Process), TRUE, FALSE, FALSE);
	init_queue(&level4, sizeof(Process), TRUE, FALSE, FALSE);
	init_queue(&terminated, sizeof(Process), TRUE, FALSE, FALSE);
	Process nullProc = {0,0,0,0,0,0,0,0,0,0,0,0,READY,0,0};
	Process procExecuting = {0,0,0,0,0,0,0,0,0,0,0,0,VOID,0,0};
	int pExecuting;
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
			printf("CREATE: Process %lu entered the ready queue at time %lu.\n", 
					procArriving.PID, clock);
			delete_current(&notArrived);
			procArriving.state = READY;
			procArriving.quantum = 10;
			procArriving.quantumRemaining = 10;
			procArriving.inWhichQueue = 1;
			add_to_queue(&level1, &procArriving, 0);
			element++;

			if(!peek_at_current(&notArrived, &procArriving, 0))
				break;
		}

		// Section 2.2: Handles choosing process to execute.
		// If empty, null process will tick.
		if(!readyProcess(&level1,&level2,&level3,&level4)) {
			nullProc.usageCPU = nullProc.usageCPU + 1;
		}
		else {
			// If nothing is executing, grab process in front and begin execution.
			if(procExecuting.PID == 0) {
				rewind_queue(&level1);
				//peek_at_current(&level1, &procExecuting, &pExecuting);
				grabReadyProcess(&level1,&level2,&level3,&level4,&procExecuting);
				//if(procExecuting.IORemaining != 0) {
					printf("RUN: Process %lu started execution from level %d at time %lu ",
							procExecuting.PID, procExecuting.inWhichQueue, clock);
					printf("wants to execute for %lu ticks. Repeat: %lu\n", 
							procExecuting.burstRemaining, procExecuting.repeat);
				//}
				// trickle down burst one tick at a time
				if(procExecuting.burstRemaining > 0) {
					procExecuting.burstRemaining = procExecuting.burstRemaining - 1;
					procExecuting.quantumRemaining--;
					
				//	if(procExecuting.quantumRemaining == 0) {
				//		procExecuting.b++;
				//		procExecuting.quantumRemaining = procExecuting.quantum;
						//printf("PID: %lu, BR: %lu Q: %d\n", 
						//procExecuting.PID,procExecuting.burstRemaining,
						//procExecuting.inWhichQueue);
				//	}
					// procExecuting.usageCPU++ or something... incorporate usage tracking.
				}
				// burst must be depleted, check if i/o needs to be done, if not then tick down repeat.
				else {
					if(procExecuting.IORemaining > 0) {
						// I/O needs to be done, remove processes from level1 queue
						// and move them into the blocked queue.
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
								printf("ERROR: process is lost");
								break;
						}
						procExecuting = nullProc;
						
					}
					// No burst or IO, so check if repeat needs to be done. 					
					else {
						//printf("Hello");
						//if(procExecuting.repeat > 0) {
						//	procExecuting.repeat = procExecuting.repeat - 1;
						//	procExecuting.burstRemaining = procExecuting.burst;
						//	procExecuting.IORemaining = procExecuting.IO;
						//}

						//else {
							// no burst, io, or repeat left so terminate the process.
							// multi level system cant just delete from level 1 fix it
							printf("FINISHED: Process %lu finished at time %lu.\n",
									procExecuting.PID, clock);
							delete_current(&level1);
							add_to_queue(&terminated, &procExecuting, 0);
							procExecuting = nullProc;
						//}
					}
				}
			}
			// If something is executing, then check if a higher priority process exists.
			// (NOT IMPLEMENTED YET)
			
			// If not, then just tick down burst or IO or repeat.
			else {
				
				//procExecuting = nullProc;
				//grabReadyProcess(&level1, &level2, &level3, &level4, &procExecuting);
				// Burst
				if(procExecuting.burstRemaining > 0) {
					procExecuting.burstRemaining = procExecuting.burstRemaining - 1;
					procExecuting.quantumRemaining--;
					
				//	if(procExecuting.quantumRemaining == 0) {
				//		procExecuting.b++;
				//		procExecuting.quantumRemaining = procExecuting.quantum;
						//printf("PID: %lu, BR: %lu Q: %d\n", 
						//procExecuting.PID,procExecuting.burstRemaining,
						//procExecuting.inWhichQueue);
				//	}
				}

				else {
					// IO
					if(procExecuting.IORemaining > 0) {
						rewind_queue(&level1);
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
								printf("ERROR: process is lost");
								break;
						}
						procExecuting = nullProc;
					}
					// No burst or IO, so check repeat
					else {
						//if(procExecuting.repeat > 0) {
						//	procExecuting.repeat = procExecuting.repeat - 1;
						//	procExecuting.burstRemaining = procExecuting.burst;
						//	procExecuting.IORemaining = procExecuting.IO;
						//}
						
						//else {
							// no burst, io, or repeat left so its terminated.
							// multiqueue so cant just delete from level 1 fix this
							printf("Hello");
							printf("FINISHED: Process %lu finished at time %lu.\n",
									procExecuting.PID, clock);
							add_to_queue(&terminated, &procExecuting, 0);
							delete_current(&level1);
							procExecuting = nullProc;
						//}	
					}	
				}
			}
		}

		// Section 2.3: handles Exit/IO/Promotion/Demotion
		// START HERE
		
		// FOR I/O, all processes in the blocked queue needs to get one tick of their I/O completed.
		if(!empty_queue(&blocked)) {
			rewind_queue(&blocked);
			while(!end_of_queue(&blocked)) {
				Process *curr = (Process *) pointer_to_current(&blocked);
				
				if(curr->IORemaining == 0) {
					//printf("QUEUED: Process %lu queued at level %d at time %lu.\n",
					//	       	curr->PID, curr->inWhichQueue, clock);
					curr->b = 0;
					if(curr->repeat > 1) {
						curr->repeat = curr->repeat - 1;
						curr->burstRemaining = curr->burst;
						curr->IORemaining = curr->IO;
						curr->quantumRemaining = curr->quantum;
					}
					else {
						curr->repeat = curr->repeat - 1;
						curr->burstRemaining = curr->burst;
						curr->quantumRemaining = curr->quantum;
					}

					switch(curr->inWhichQueue) {
						case 1:
							add_to_queue(&level1, curr, 0);
							break;
						case 2:
							add_to_queue(&level2, curr, 0);
							break;
						case 3:
							add_to_queue(&level3, curr, 0);
							break;
						case 4:
							add_to_queue(&level4, curr, 0);
							break;
						default:
							printf("ERROR: lost track of where process belonged");
							exit(0);
							break;
					}
					
					delete_current(&blocked);
					// rewind_queue(&blocked);
				}
				else {
					//printf("%lu %lu %lu ", curr.PID, curr.burstRemaining, curr.IORemaining);
					curr->IORemaining = curr->IORemaining - 1;
					//printf("%lu %lu %lu\n", curr.PID, curr.burstRemaining, curr.IORemaining);	
				}
				
				if(!end_of_queue(&blocked)) {
					next_element(&blocked);
				}
			}
		}

		// Do promotion and demotion decisions.
		// BROKEN
		/* Queue* currQ; */
		/* Queue* nextQ; */
		/* if(procExecuting.PID != 0 && */ 
		/* 		procExecuting.b == demotionCheck(&level1, &level2, &level3, &level4, &procExecuting, currQ, nextQ)) { */	
		/* 	printf("QUEUED: Process %lu queued at level %d at time %lu\n", */
		/* 			procExecuting.PID, ++procExecuting.inWhichQueue, clock); */
		/* 	rewind_queue(currQ); */
		/* 	delete_current(currQ); */
		/* 	add_to_queue(nextQ,&procExecuting,0); */
		/* } */
		if(procExecuting.PID != 0) {
			int countedUp = 0;
			if(procExecuting.quantumRemaining == 0) {	
				procExecuting.b++;
				procExecuting.quantumRemaining = procExecuting.quantum;
				countedUp++;
			}
			switch(procExecuting.inWhichQueue) {
				case 1:
					if(procExecuting.b == 1) {
					 	printf("QUEUED: Process %lu queued at level %d at time %lu BR: %lu IOR: %lu.\n", 
			 			procExecuting.PID, ++procExecuting.inWhichQueue, clock,
						procExecuting.burstRemaining, procExecuting.IORemaining);
						procExecuting.b = 0;
						procExecuting.quantum = 30;
					    	procExecuting.quantumRemaining = 30;	
						rewind_queue(&level1);
						add_to_queue(&level2, &procExecuting, 0);
						delete_current(&level1);
						procExecuting = nullProc;
					}
					break;
				case 2:
					//printf("PID: %lu, Q: %d B: %d\n", 
					//procExecuting.PID,procExecuting.inWhichQueue,procExecuting.b);
					if(procExecuting.b == 2) {
						printf("QUEUED: Process %lu queued at level %d at time %lu.\n",
						procExecuting.PID, ++procExecuting.inWhichQueue, clock);
						procExecuting.b = 0;
						procExecuting.quantum = 100;
					    	procExecuting.quantumRemaining = 100;	
						rewind_queue(&level2);
						add_to_queue(&level3, &procExecuting, 0);
						delete_current(&level2);
						procExecuting = nullProc;
					}
					else if(countedUp) {
						printf("QUEUED: Process %lu queued at level %d at time %lu.\n",
						procExecuting.PID, procExecuting.inWhichQueue, clock);
						rewind_queue(&level2);
						delete_current(&level2);
						add_to_queue(&level2, &procExecuting, 0);
						procExecuting = nullProc;
					}
					break;
				case 3:
					if(procExecuting.b == 2) {
						printf("QUEUED: Process %lu queued at level %d at time %lu.\n",
						procExecuting.PID, ++procExecuting.inWhichQueue, clock);
						procExecuting.b = 0;
						procExecuting.quantum = 200;
					    	procExecuting.quantumRemaining = 200;	
						rewind_queue(&level3);
						add_to_queue(&level4, &procExecuting, 0);
						delete_current(&level3);
						procExecuting = nullProc;
					}
					else if(countedUp) {
						printf("QUEUED: Process %lu queued at level %d at time %lu.\n",
						procExecuting.PID, procExecuting.inWhichQueue, clock);
						rewind_queue(&level3);
						delete_current(&level3);
						add_to_queue(&level3, &procExecuting, 0);
						procExecuting = nullProc;
					}
					break;
				case 4:
					if(countedUp) {
						printf("QUEUED: Process %lu queued at level %d at time %lu.\n",
						procExecuting.PID, procExecuting.inWhichQueue, clock);
						rewind_queue(&level4);
						delete_current(&level4);
						add_to_queue(&level4, &procExecuting, 0);
						procExecuting = nullProc;
					}
					break;
				default:
					printf("ERROR: process is lost.");
					break;
			}
			
		}

		if(procExecuting.PID==0 && grabReadyProcess(&level1,&level2,&level3,&level4,&procExecuting)) {
			printf("RUN: Process %lu started execution from level %d at time %lu ",
					procExecuting.PID, procExecuting.inWhichQueue, clock);
			printf("wants to execute for %lu.\n", procExecuting.burstRemaining);
		}

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
	
	// WILL BE REMOVED JUST CHECKING IF ALL THE PROCESSES LEFT THE BLOCKED QUEUE/STATE
	// SHOULD CONTAIN NO PROCESSES
	printf("\n[*] Printing Blocked Queue...\n");
	element=1;
	rewind_queue(&blocked);
	while (!end_of_queue(&blocked)) {
		printf("Element: %lu PID: %lu Time: %lu I/O Remain: %lu I/O: %lu\n",
				element++,
				((Process *) pointer_to_current(&blocked))->PID,
				((Process *) pointer_to_current(&blocked))->arrivalTime,
				((Process *) pointer_to_current(&blocked))->IORemaining,
				((Process *) pointer_to_current(&blocked))->IO);
		next_element(&blocked);
	}

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

int processesExist(Queue *a, Queue *b, Queue *one, Queue *two, Queue *three, Queue *four) {
	
	// Fix this, it needs !empty_queue(b)
	if(!empty_queue(a) || !empty_queue(b) || !empty_queue(one) || !empty_queue(two) || !empty_queue(three) || !empty_queue(four)) {
		return 1;
	}
	else {
		return 0;
	}
}

int readyProcess(Queue *one, Queue *two, Queue *three, Queue *four) {
	
	if(!empty_queue(one) || !empty_queue(two) || !empty_queue(three) || !empty_queue(four)) {
		return 1;
	}
	else {
		return 0;
	}

}

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

int demotionCheck(Queue* one, Queue* two, Queue* three, Queue* four, Process* proc, Queue* currQ, Queue *nextQ) {

	if(proc->PID == 0) {
		return 0;
	}

	switch(proc->inWhichQueue) {
		case 1:
			currQ = &(*one);
			nextQ = &(*two);
			return 1;
			break;
		case 2:
			currQ = &(*two);
			nextQ = &(*three);
			return 2;
			break;
		case 3:
			currQ = &(*three);
			nextQ = &(*four);
			return 2;
			break;
		case 4:
			currQ = &(*four);
			nextQ = &(*four);
			return -1;
			break;
		default:
			printf("ERROR: process is lost.");
			return -2;
			break;
	}
}


