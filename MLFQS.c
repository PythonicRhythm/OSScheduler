
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
// one behavior finishes it rewrites the first element with
// the second elements behavior and points to the third one.
//
// Queue level1: [P1, P2, P3, P4, ....]
//						  ^^
//						  P3
//						  ^^
//						  P3

// FUTURE CORRECTIONS:
// 
// This might make me lose points but... my IO is incorrect.
// Processes are kicked out of their queue when its IO time.
// They are supposed to keep their spot and then return but
// I found out too late so my structure is incorrect.
//
// TODO: Processes keep their spot when they reach IO phase instead of getting kicked off.
// TODO: After grading, change output to be more visually clear.
// TODO: Create more methods for tasks that i've copied a million times.

// ALL FUNCTION DECLARATIONS
// go to the actual methods for better explanation of use.
int processesExist();
int readyProcessExists();
void* grabAReadyProcess(Process*);
void init_all_queues();
void deleteFromQ(Process*);
void demoteProcess(Process*);
void demotionAndPromotionCheck(Process*);
void insertAtRear(Process*);
void updateValues(Process*);

// ALL GLOBAL VARIABLES OR STRUCTS
Queue preScheduleProcs;	// Queue that stores all the input processes pre-Schedule.
Queue blocked;			// Queue that stores all the processes blocked for IO.
Queue level1;			// The Level 1 Queue for the MLFQS
Queue level2;			// The Level 2 Queue for the MLFQS
Queue level3;			// The Level 3 Queue for the MLFQS
Queue level4;			// The Level 4 Queue for the MLFQS
Queue terminated;		// Queue that stores all the terminated processes.
Process nullProc = {0,0};			// <<NULL>> process that ticks when scheduler empty.
Process currExecuting = {0,0};		// Process that is currently executing.
unsigned long schedClock=0;			// The clock used to keep track of ticks.

int main(int argc, char *argv[]) {

	// Initializing All Queues.
	init_all_queues();

	// INPUT COLLECTION SECTION:
	// works best with piping input via a txt file.
	// EX: ./myTest < input-text.txt
	Process newProcess;
	Process *prevP;
	while (scanf("%lu %lu %lu %lu %lu", &(newProcess.arrivalTime), &(newProcess.PID), 
	&(newProcess.burst), &(newProcess.IO), &(newProcess.repeat)) == 5) {
		// Set up basic variables
		newProcess.burstRemaining = newProcess.burst;
		newProcess.IORemaining = newProcess.IO;
		newProcess.usageCPU = 0;
		newProcess.nextSet = NULL;
		if(!empty_queue(&preScheduleProcs)) {
			// If new process is the same PID as the previous one,
			// make the previous point to the new process to set up
			// the linked list system.
			if(newProcess.PID == prevP->PID) {
				Process* nextBehavior = (Process *) malloc(sizeof(Process));
				nextBehavior->burst = newProcess.burst;
				nextBehavior->burstRemaining = newProcess.burstRemaining;
				nextBehavior->IO = newProcess.IO;
				nextBehavior->IORemaining = newProcess.IORemaining;
				nextBehavior->PID = newProcess.PID;
				nextBehavior->repeat = newProcess.repeat;
				nextBehavior->arrivalTime = newProcess.arrivalTime;
				prevP->nextSet = nextBehavior;
				prevP = nextBehavior;
			}
			// If process is a new PID, then no need for the linked list system,
			// add as normal and set it as the previous process in case the new
			// processes are the same PID.
			else {
				add_to_queue(&preScheduleProcs, &newProcess, 0);
				while(!end_of_queue(&preScheduleProcs)) {
					prevP = (Process *) pointer_to_current(&preScheduleProcs);
					next_element(&preScheduleProcs);
				}
			}
			
		}
		// Essentially only runs one time, for the first process.
		// Add to the queue and set it as previous.
		else {
			add_to_queue(&preScheduleProcs, &newProcess, 0);
			prevP = (Process *) pointer_to_current(&preScheduleProcs);
		}
	}
	prevP = NULL;
	
	// THE SCHEDULER LOOP BEGINS HERE!
	//
	// STRUCTURE ORDER: 
	// SEC 1: ARRIVALS
	// SEC 2: EXECUTION
	// SEC 3: IO / PROMOTION / DEMOTION / EXIT
	// SEC 4: CLOCK TICK
	
	while(processesExist(&preScheduleProcs,&blocked,&level1,&level2,&level3,&level4)) {

		// currArriving will keep track of the current arriving process.
		Process currArriving;
		rewind_queue(&preScheduleProcs);
		peek_at_current(&preScheduleProcs, &currArriving, 0);
		
		/// SECTION 1: ARRIVALS
		// This section checks the arrTime of the processes stored in "preScheduleProcs"
		// and if it matches the clock, processes are sent to the level 1 queue.
		// If it doesnt match, move to the execution section of the scheduler.
		if(currArriving.arrivalTime == schedClock) {
			printf("PID: %lu, ARRIVAL TIME: %lu\n",
			currArriving.PID, currArriving.arrivalTime);
			printf("CREATE: Process %lu entered the ready queue at time %lu.\n", 
					currArriving.PID, schedClock);
			// Remove from the preSchedule queue and
			// update with level 1 queues b, g, quantum values.
			delete_current(&preScheduleProcs);
			currArriving.quantum = 10;
			currArriving.quantumRemaining = 10;
			currArriving.inWhichQueue = 1;
			currArriving.gLim = -1;
			currArriving.bLim = 1;
			currArriving.g = 0;
			currArriving.b = 0;
			add_to_queue(&level1, &currArriving, 0);
		}

		// SECTION 2: EXECUTION
		// This section represents the execution phase of a process.
		// The process will tick down burst and quantum, if burst
		// reaches 0 then send to IO, if quantum remaining equals
		// 0 then either send to rear or demote if "bad" limit reached.
		// Demotion is handled in Execution but we check in IO for
		// specific cases like expending all quantum perfectly when burst
		// is 0. Promotion is only handled in IO Section.

		// If empty, null process will tick.
		if(!readyProcessExists(&level1,&level2,&level3,&level4)) {
			nullProc.usageCPU = nullProc.usageCPU + 1;
		}
		// If no process running but there exists some ready
		// processes, then set highest process as running process.
		else if(currExecuting.PID == 0) {
				grabAReadyProcess(&currExecuting);
				printf("RUN: Process %lu started execution from level %d at time %lu; ",
				currExecuting.PID, currExecuting.inWhichQueue, schedClock);
				printf("wants to execute for %lu ticks.\n", 
				currExecuting.burstRemaining);
		}
		// If a process is currently running, then continue execution
		else {
			
			currExecuting.burstRemaining--;
			currExecuting.quantumRemaining--;
			currExecuting.usageCPU++;
			
			// If burst is 0, check if IO needs to be done or process is finished
			if(currExecuting.burstRemaining == 0) {

				// If process has IO but no burst, then send to IO buffer to tick
				// and remove from process from queue then set currExecuting to nullProc.
				// Reset bad behavior and tick up good behavior if well behaved.
				if(currExecuting.IORemaining > 0) {

					if(currExecuting.quantumRemaining == 0) {currExecuting.b++;}
					else {
						if(currExecuting.b == 0) {currExecuting.g++;}
						else {currExecuting.b = 0;}
					}
					// Send to IO and delete from current level.
					currExecuting.quantumRemaining = currExecuting.quantum;
					printf("I/O: Process %lu blocked for I/O at time %lu.\n", 
						currExecuting.PID, schedClock);
					add_to_queue(&blocked, &currExecuting, 0);
					deleteFromQ(&currExecuting);
					currExecuting = nullProc;
				}
				// If no burst and IO, then process is finished and must be terminated.
				else {
					printf("FINISHED: Process %lu finished at time %lu.\n",
						currExecuting.PID, schedClock);
					add_to_queue(&terminated, &currExecuting, 0);
					deleteFromQ(&currExecuting);
					currExecuting = nullProc;
				}

			}
			// If burst is not 0, check if quantum was met
			else if(currExecuting.quantumRemaining == 0) {
				currExecuting.b++;
				currExecuting.g = 0;

				// Demotion checking, if b = bLim (b's limit for queue level) then demote.
				if(currExecuting.b == currExecuting.bLim) {
					currExecuting.inWhichQueue++;
					printf("QUEUED: Process %lu queued at level %d at time %lu.\n",
					currExecuting.PID, currExecuting.inWhichQueue, schedClock);
					// In demotion & promotion, I set the b, g, and quantum requirements.
					demoteProcess(&currExecuting);
					currExecuting = nullProc;
				}
				else {
					// If b was counted up but it is not enough to demote, then we put
					// the current executing process at the back of the current queue.
					currExecuting.quantumRemaining = currExecuting.quantum;
					printf("QUEUED: Process %lu queued at level %d at time %lu.\n",
					currExecuting.PID, currExecuting.inWhichQueue, schedClock);
					insertAtRear(&currExecuting);
					currExecuting = nullProc;
				}
			}
			

			// If no process is in a running state after execution actions, 
			// check if there exists a ready process and if so, set it
			// as executing process and print running message.
			if(currExecuting.PID == 0) {

				if(readyProcessExists(&level1,&level2,&level3,&level4)) {

					grabAReadyProcess(&currExecuting);
					printf("RUN: Process %lu started execution from level %d at time %lu; ",
					currExecuting.PID, currExecuting.inWhichQueue, schedClock);
					printf("wants to execute for %lu ticks.\n",
					currExecuting.burstRemaining);

				}

			}
			// If process is currently executing, check if there exists a higher
			// level process and if so, set the higher level process as currently
			// running.
			else {

				Process temp;
				grabAReadyProcess(&temp);

				if(currExecuting.inWhichQueue > temp.inWhichQueue) {

					printf("QUEUED: Process %lu queued at level %d at time %lu.\n",
					currExecuting.PID, currExecuting.inWhichQueue, schedClock);
					updateValues(&currExecuting);
					grabAReadyProcess(&currExecuting);
					printf("RUN: Process %lu started execution from level %d at time %lu; ",
					currExecuting.PID, currExecuting.inWhichQueue, schedClock);
					printf("wants to execute for %lu ticks.\n",
					currExecuting.burstRemaining);
				}

			}
				
		}
		

		// Section 3: IO / Promotion / Demotion / Exit
		// This section will represent the IO buffer for
		// the scheduler. All processes in the blocked
		// queue will be ticked down one IO. Handles
		// promotion and demotion for a specific case.
		if(!empty_queue(&blocked)) {

			rewind_queue(&blocked);
			while(!end_of_queue(&blocked)) {

				Process *curr = (Process *) pointer_to_current(&blocked);
				curr->IORemaining--;
				
				// If a process has no remaining IO, return them to their queue.
				if(curr->IORemaining == 0) {
					
					curr->repeat--;
					// If repeat is 0, check if a process has "child"
					// behaviors. If so, reset process with new behaviors,
					// else it just returns with one phase.
					// If repeat isnt 0, just return the process with
					// reset burst and IO.
					if(curr->repeat == 0) {

						if(curr->nextSet != NULL && curr->nextSet->PID == curr->PID) {
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

					demotionAndPromotionCheck(curr);
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

		// EXIT CHECK
		// If the last process finished its execution, close the scheduler, we are done!
		if(!processesExist(&preScheduleProcs,&blocked,&level1,&level2,&level3,&level4)) {
			break;
		}

		schedClock++;
	}

	// FINAL OUTPUT SECTION
	printf("Scheduler shutdown at time %lu.\n", schedClock);
	printf("Total CPU usage for all processes scheduled:\n");
	printf("Process <<null>>:\t%lu time units.\n", nullProc.usageCPU);
	rewind_queue(&terminated);
	while(!end_of_queue(&terminated)) {
		Process *curr = (Process *) pointer_to_current(&terminated);
		printf("Process %lu:\t\t%lu time units.\n", curr->PID, curr->usageCPU);
		next_element(&terminated);
	}

}

// init_all_queues() will initialize all the queues that we will
// be using for the scheduler.
void init_all_queues() {
	init_queue(&preScheduleProcs, sizeof(Process), TRUE, FALSE, FALSE);
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
	
	if(!empty_queue(&preScheduleProcs) || !empty_queue(&blocked) || !empty_queue(&level1) || !empty_queue(&level2) || !empty_queue(&level3) || !empty_queue(&level4)) {
		return 1;
	}
	else {
		return 0;
	}
}

// readyProcessExists() checks if level 1-4 queues contain
// any processes that are ready for execution. 
// Returns 1 if TRUE, 0 if FALSE. 
int readyProcessExists() {
	
	if(!empty_queue(&level1) || !empty_queue(&level2) || !empty_queue(&level3) || !empty_queue(&level4)) {
		return 1;
	}
	else {
		return 0;
	}

}

// grabAReadyProcess() grabs the highest level process that
// is currently ready. It copies the data from the ready
// process to Process *proc.
// Returns a NULL pointer if no ready process,
// else returns pointer to element.
void* grabAReadyProcess(Process *proc) {

	// HOW IT WORKS: 
	// checks the first element of level 1, if not NULL,
	// grabs the element sets it as proc and returns 1.
	// If it is NULL then you do the same process for
	// level 2 and onward.

	void *ele = NULL;
	if(pointer_to_current(&level1) != NULL) {
		rewind_queue(&level1);
		peek_at_current(&level1, proc, 0);
		ele = (void *) proc;
		return ele;
	}
	else if(pointer_to_current(&level2) != NULL) {		
		rewind_queue(&level2);
		peek_at_current(&level2, proc, 0);
		ele = (void *) proc;
		return ele;
	}
	else if(pointer_to_current(&level3) != NULL) {
		rewind_queue(&level3);
		peek_at_current(&level3, proc, 0);
		ele = (void *) proc;
		return ele;
	}
	else if(pointer_to_current(&level4) != NULL) {
		rewind_queue(&level4);
		peek_at_current(&level4, proc, 0);
		ele = (void *) proc;
		return ele;
	}
	else {
		return ele;
	}
}

// delFromQ() will determine the queue that toBeDel is contained in,
// and delete it from that queue.
void deleteFromQ(Process *toBeDel) {

	switch(toBeDel->inWhichQueue) {
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
}

// demotionProcess() will check what level the process is in,
// update the requirements for b, g, and quantum to match new
// level, add it to the new level queue, and then delete
// it from the old level.
void demoteProcess(Process *toBeDemoted) {

	switch(toBeDemoted->inWhichQueue) {
		case 2:
			toBeDemoted->b = 0;
			toBeDemoted->bLim = 2;
			toBeDemoted->gLim = 1;
			toBeDemoted->quantum = 30;
			toBeDemoted->quantumRemaining = 30;
			add_to_queue(&level2, toBeDemoted, 0);
			delete_current(&level1);
			break;
		case 3:
			toBeDemoted->b = 0;
			toBeDemoted->bLim = 2;
			toBeDemoted->gLim = 2;
			toBeDemoted->quantum = 100;
			toBeDemoted->quantumRemaining = 100;
			add_to_queue(&level3, toBeDemoted, 0);
			delete_current(&level2);
			break;
		case 4:
			toBeDemoted->b = 0;
			toBeDemoted->bLim = -1;
			toBeDemoted->gLim = 2;
			toBeDemoted->quantum = 200;
			toBeDemoted->quantumRemaining = 200;
			add_to_queue(&level4, toBeDemoted, 0);
			delete_current(&level3);
			break;
		default:
			printf("ERROR: Process is lost.\n");
			exit(0);
			break;
	}

}

// demotionAndPromotionCheck() will check the queue that curr is in
// and check if it needs to be demoted or promoted based on the
// requirements of the level that contains the curr process.
void demotionAndPromotionCheck(Process *curr) {

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

}

// insertAtRear() inserts process given to the rear of the
// queue that contains it. Used for demotion counter.
void insertAtRear(Process* toBePutRear) {
	switch(toBePutRear->inWhichQueue) {
		case 1:
			rewind_queue(&level1);
			add_to_queue(&level1, toBePutRear, 0);
			delete_current(&level1);
			break;
		case 2:
			rewind_queue(&level2);
			add_to_queue(&level2, toBePutRear, 0);
			delete_current(&level2);
			break;
		case 3:
			rewind_queue(&level3);
			add_to_queue(&level3, toBePutRear, 0);
			delete_current(&level3);
			break;
		case 4:
			rewind_queue(&level4);
			add_to_queue(&level4, toBePutRear, 0);
			delete_current(&level4);
			break;
		default:
			printf("ERROR: Process is lost.\n");
			exit(0);
			break;
	}
}

// updateValues() updates the values from toBeUpdated to adjustments.
// This method is used when an executing process is kicked off because
// of a higher priority process thats ready. This is used because the
// currExecuting process variable is not directly connected to the process
// its representing in the queue.
void updateValues(Process* toBeUpdated) {
	Process *adjustments = NULL;
	switch(toBeUpdated->inWhichQueue) {
		case 1:
			adjustments = pointer_to_current(&level1);
			adjustments->burstRemaining = toBeUpdated->burstRemaining;
			adjustments->b = toBeUpdated->b;
			adjustments->g = toBeUpdated->g;
			adjustments->quantumRemaining = toBeUpdated->quantumRemaining;
			adjustments->usageCPU = toBeUpdated->usageCPU;
			break;
		case 2:
			adjustments = pointer_to_current(&level2);
			adjustments->burstRemaining = toBeUpdated->burstRemaining;
			adjustments->b = toBeUpdated->b;
			adjustments->g = toBeUpdated->g;
			adjustments->quantumRemaining = toBeUpdated->quantumRemaining;
			adjustments->usageCPU = toBeUpdated->usageCPU;
			break;
		case 3:
			adjustments = pointer_to_current(&level3);
			adjustments->burstRemaining = toBeUpdated->burstRemaining;
			adjustments->b = toBeUpdated->b;
			adjustments->g = toBeUpdated->g;
			adjustments->quantumRemaining = toBeUpdated->quantumRemaining;
			adjustments->usageCPU = toBeUpdated->usageCPU;
			break;
		case 4:
			adjustments = pointer_to_current(&level4);
			adjustments->burstRemaining = toBeUpdated->burstRemaining;
			adjustments->b = toBeUpdated->b;
			adjustments->g = toBeUpdated->g;
			adjustments->quantumRemaining = toBeUpdated->quantumRemaining;
			adjustments->usageCPU = toBeUpdated->usageCPU;
			break;
		default:
			printf("ERROR: process is lost.\n");
			exit(0);
			break;
	}
}