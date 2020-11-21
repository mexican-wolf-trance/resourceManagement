#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <signal.h>
#include <time.h>

#define BUFSIZE 1024
#define SEC_KEY 0x1234567
#define MSG_KEY 0x2345
#define RES_KEY 0x7654
#define SEM_KEY 0x1111


//Trying out the semaphore
union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

struct sembuf p = { 0, -1, SEM_UNDO};
struct sembuf v = { 0, +1, SEM_UNDO};

//Initialize the message and clock structs!
struct msgbuf
{
	long mtype;
	int mresReq;
	int mresNo;
	pid_t pid;
} message;

typedef struct Clock
{
	unsigned int sec;
	unsigned int nsec;
//	pid_t shmPID;
} Clock;

typedef struct Resources
{
	int usedResources[20];
} Resources;

typedef struct PCB
{
	pid_t pid;
	unsigned int totalBlockedTime;
	unsigned int blockedBurstTime;
	int resReq;
	Resources *used;
} PCB;

typedef struct Queue
{
	struct Queue *next;
	struct PCB *head;
} Queue;

//I intitalize the ids and the clock pointer here so my signal handler can use them
int shmid, msgqid, resid, semid;
struct Clock *sim_clock;
struct Clock *new_proc_clock;
union semun u;
FILE *fp;

Resources *totalResources;
Resources *availableResources;
Resources *allocatedResources;

Queue *blockedQueue;
Queue *processList;

PCB *new_pcb(pid_t pid)
{
        PCB *pcb  = malloc(sizeof(PCB));
	pcb->pid = pid;
        pcb->totalBlockedTime = 0;
        pcb->blockedBurstTime = 0;
        pcb->resReq = 0;
        pcb->used = malloc(sizeof(Resources));
        int j;
        for(j = 0; j < 20; j++)
                pcb->used->usedResources[j] = 0;
 
        return pcb;
}

void queuePush(Queue** head_ref, pid_t pid)
{
	Queue* new_proc = malloc(sizeof(Queue));
	new_proc->head = malloc(sizeof(PCB));
	new_proc->head = new_pcb(pid);
	new_proc->next = (*head_ref);
	(*head_ref) = new_proc;
}

void blockedPush(Queue** head_ref, PCB *pcb)
{
//        Queue* new_proc = malloc(sizeof(Queue));
//	Queue* last = *head_ref;
//        new_proc->head = malloc(sizeof(PCB));
//        new_proc->head = pcb;
//        new_proc->next = NULL;
//	if(*head_ref == NULL)
//	{
//		*head_ref = new_proc;
//		return;
//	}
//        while(last->next != NULL)
//		last = last->next;
//	last->next = new_proc;
//	return;
        Queue* new_proc = malloc(sizeof(Queue));
//        new_proc->head = malloc(sizeof(PCB));
        new_proc->head = pcb;
        new_proc->next = (*head_ref);
        (*head_ref) = new_proc;

}

void deleteProc(Queue** ptr, pid_t pid)
{
	Queue *temp = *ptr, *prev;
	while(temp != NULL && temp->head->pid == pid)
	{
		*ptr = temp->next;
		free(temp);
		return;
	}
	while(temp != NULL && temp->head->pid != pid)
	{
		prev = temp;
		temp = temp->next;
	}
	if(temp == NULL)
		return;
	prev->next = temp->next;
	free(temp);
}
	
PCB *findProc(Queue *ptr, pid_t pid)
{
//	printf("searching for a PCB %ld\n", (long) pid);
	while(ptr != NULL)
	{
		if(ptr->head->pid == pid)
			return ptr->head;
		else
			ptr = ptr->next;
	}
	return NULL;
}

//set next process fork time
void newProcTime()
{
	new_proc_clock->nsec = sim_clock->nsec + ((rand() % 500) * 100000);
	new_proc_clock->sec = sim_clock->sec;
}
//check if time for a new process to begin
int checkProcTime()
{
	if((new_proc_clock->nsec + new_proc_clock->sec * 1000000000) <= (sim_clock->nsec + sim_clock->sec * 1000000000))
	{
		newProcTime();
		return 1;
	}
	return 0;
}

//The signal handler! Couldn't figure out how to close the file stream before getting signal though
void sigint(int sig)
{
	fclose(fp);
        printf("\nChildren still in process list: ");
        while(processList != NULL)
	{
                printf("%ld, ", (long) processList->head->pid);
		processList = processList->next;
	}
        printf("\n");

	printf("\nChildren still in blocked queue: ");
        while(blockedQueue != NULL)
        {
                printf("%ld, ", (long) blockedQueue->head->pid);
                blockedQueue = blockedQueue->next;
        }
        printf("\n");

        if (msgctl(msgqid, IPC_RMID, NULL) == -1)
                fprintf(stderr, "Message queue could not be deleted\n");
	shmdt(totalResources);
	shmctl(resid, IPC_RMID, NULL);
        shmdt(sim_clock);
        shmctl(shmid, IPC_RMID, NULL);
	semctl(semid, 0, IPC_RMID, u);
	if(sig == SIGALRM)
		write(1, "\nAlarm! Alarm!\n", 15);
	else if(sig == SIGSEGV)
		write(1, "\nWoops! You got a segmentation fault!\n", 38);
	else if(sig == SIGINT)
		write(1, "\nCTRL C was hit!\n", 17);
	else
		write(1, "Oh no! An error!\n", 17);
	write(1, "Now killing the kiddos\n", 23);
	kill(0, SIGQUIT);
}


//The main function
int main (int argc, char **argv)
{
	//The CTRL C catch
	signal(SIGINT, sigint);
	signal(SIGSEGV, sigint);

	fp = fopen("log.out", "w");

	int i, option, max_child = 18, max_time = 30, counter = 0, tot_proc = 0, vOpt = 0, resCheck;
	char *exec[] = {"./user", NULL};
	pid_t child = 0;
	//Getopt is great!
        while ((option = getopt(argc, argv, "hv")) != -1)
        switch (option)
        {
                case 'h':
                        printf("This is the operating system simulator!\n");
                        printf("Usage: ./oss\n");
                        printf("For a more verbose output file, add the -v option\n");
                        printf("Enjoy!\n");
                        return 0;
                case 'v':
                        vOpt = 1;
                        break;
                case '?':
                        printf("%c is not an option. Use -h for usage details\n", optopt);
                        return 0;
        }
        if (argc > 2)
        {
                printf("Invalid usage! Check the readme\nUsage: ./oss\n");
                printf("Use the -v option for a more verbose log file");
                return 0;
        }

	srand(time(NULL) + getpid());
	//Get my shared memory and message queue keys
	shmid = shmget(SEC_KEY, sizeof(Clock), 0644 | IPC_CREAT);
	if (shmid == -1)
	{
		perror("shmid get failed");
		return 1;
	}
	
	//Put the clock in shared memory
	sim_clock = (Clock *) shmat(shmid, NULL, 0);
	if (sim_clock == (void *) -1)
	{
		perror("clock get failed");
		return 1;
	}
	//Total resrouces key
	resid = shmget(RES_KEY, sizeof(Resources), 0644 | IPC_CREAT);
	if(resid == -1)
	{
		perror("resid get failed");
		return 1;
	}
	//Place total resources in shared memory
	totalResources = (Resources *) shmat(resid, NULL, 0);
	if(totalResources ==(void *) -1)
	{
		perror("total resources get failed");
		return 1;
	}

	//Message queue key
	msgqid = msgget(MSG_KEY, 0644 | IPC_CREAT);
        if (msgqid == -1)
        {
                perror("msgqid get failed");
                return 1;
        }
	
	semid = semget(SEM_KEY, 1, 0644 | IPC_CREAT);
	if(semid == -1)
	{
		perror("semid get failed");
		return 1;
	}
	//Begin the alarm. Goes off after the -t amount of time expires
	alarm(max_time);
	signal(SIGALRM, sigint);

	//Open the log file

	//Start the clock!
	sim_clock->sec = 0;
	sim_clock->nsec = 0;

	if((new_proc_clock = malloc(sizeof(Clock))) == NULL)
	{
        	perror("malloc failed");
        	return 0;
	}
	
	u.val = 1;
	if(semctl(semid, 0, SETVAL, u) < 0)
	{
		perror("semctl fail"); 
		sigint(0);
	}

//	Queue *current = NULL;
	Queue *previous = NULL;
	processList = NULL;
	blockedQueue = NULL;

	availableResources = malloc(sizeof(Resources));
	allocatedResources = malloc(sizeof(Resources));
	printf("Available resources: ");
	for(i = 0; i < 20; i++)
	{
		totalResources->usedResources[i] = (rand() % 9) + 1;
		availableResources->usedResources[i] = totalResources->usedResources[i];
		allocatedResources->usedResources[i] = 0;
		printf("%d: %d ", i, availableResources->usedResources[i]);
	}
	printf("\n");

	newProcTime();
	//Now we start the main show
	while(1)
	{
		if(semop(semid, &p, 1) < 0)
		{
            	    	perror("semop p"); 
			sigint(0);
		}
		if(counter < max_child && checkProcTime())
		{
                        if ((child = fork()) == 0)
                        {
                                execvp(exec[0], exec);
                                perror("exec failed");
                        }
        
                        if (child < 0)
                        {
                                perror("Failed to fork");
                                sigint(0);
                        }
                        counter++;
			tot_proc++;
			printf("Adding new process %ld\n", (long) child);
			queuePush(&processList, child);
			Queue *printPtr = processList;
		        printf("\nChildren still in process list: ");
			while(printPtr != NULL)
	       		{
 			        printf("%ld, ", (long) printPtr->head->pid);
	     		        printPtr = printPtr->next;
 			}
	   		printf("\n");
		
		}
                if(semop(semid, &v, 1) < 0)
		{
                    	perror("semop v"); 
			sigint(0);
		}
			
		if(msgrcv(msgqid, &message, sizeof(message), 2, IPC_NOWAIT) > 0)
		{
			printf("OSS acknowledges child %ld has died\n", (long) message.pid);
			if(vOpt)
				fprintf(fp, "OSS acknowledges child %ld has died\n", (long) message.pid);
			wait(NULL);	
			counter--;
			//printf("Total processes so far: %d\n", tot_proc);
			Queue *realPtr = processList;
			for(i = 0; i < 20; i++)
			{
				resCheck = findProc(realPtr, message.pid)->used->usedResources[i];
				availableResources->usedResources[i] += resCheck;
				allocatedResources->usedResources[i] -= resCheck;
			}
//			        printf("Available resources: ");
//		        for(i = 0; i < 20; i++)
//		        {
//		                totalResources->usedResources[i] = (rand() % 9) + 1;
//		                availableResources->usedResources[i] = totalResources->usedResources[i];
//		                allocatedResources->usedResources[i] = 0;
//	        	        printf("%d: %d ", i, availableResources->usedResources[i]);
 //       		}
//        		printf("\n");
			printf("Deleting from process list\n");
			deleteProc(&processList, message.pid);
//			Queue *delPtr = processList;				
//			while(delPtr != NULL)
//			{
//				if(delPtr->head->pid == message.pid)
//				{
//					delPtr = delPtr->next;
//					break;
//				}
//				if(delPtr->next->head->pid == message.pid)
//				{
//					delPtr->next = delPtr->next->next;
//					break;
//				}
//				delPtr = delPtr->next;
//			}
		}
		
                if(msgrcv(msgqid, &message, sizeof(message), 3, IPC_NOWAIT) > 0)
                {
                        printf("OSS acknowledges child %ld is asking for %d of resource %d which has %d left at time %u seconds %u nanoseconds\n", (long) message.pid, message.mresNo, message.mresReq, availableResources->usedResources[message.mresReq], sim_clock->sec, sim_clock->nsec);
			if(vOpt)
				fprintf(fp, "OSS acknowledges child %ld is asking %d for resource %d which has %d left at time %u seconds %u nanoseconds\n", (long) message.pid, message.mresNo, message.mresReq, availableResources->usedResources[message.mresReq], sim_clock->sec, sim_clock->nsec);
                        if(availableResources->usedResources[message.mresReq] > message.mresNo)
			{
				printf("Request granted to child %ld\n!", (long) message.pid);
				if(vOpt)
					fprintf(fp, "Request granted to child %ld\n!", (long) message.pid);
				availableResources->usedResources[message.mresReq] -= message.mresNo;
				allocatedResources->usedResources[message.mresReq] += message.mresNo;
				PCB *ptr = findProc(processList, message.pid);
				if(ptr == NULL)
				{
					printf("Process not found\n");
					sigint(0);
				}
				else
					ptr->used->usedResources[message.mresReq] += message.mresNo;
				
				message.mtype = message.pid;
                        	message.mresReq = 1;
                        	msgsnd(msgqid, &message, sizeof(message), 0);
			}
			else
			{
				PCB *newBlockProc = findProc(processList, message.pid);
				newBlockProc->blockedBurstTime = sim_clock->sec*1000000000 + sim_clock->nsec;
				newBlockProc->resReq = message.mresReq;
				newBlockProc->used->usedResources[message.mresReq] += message.mresNo;
				printf("Add child %ld to the blocked queue\n", newBlockProc->pid);
				blockedPush(&blockedQueue, newBlockProc);
				Queue *optr = blockedQueue;
			        while(optr != NULL)
			        {
			                printf("%ld, ", (long) optr->head->pid);
			                optr = optr->next;
			        }
			        printf("\n");
				printf("Request denied! Child %ld is now blocked!\n", (long) message.pid);
				fprintf(fp, "Request denied! Child %ld is now blocked!\n", (long) message.pid);
//	                        Queue *delPtr2 = processList;
	                        deleteProc(&processList, message.pid);
			        printf("\nChildren still in process list: ");
				Queue *printer = processList;
			        while(printer != NULL)
			        {
			                printf("%ld, ", (long) printer->head->pid);
			                printer = printer->next;
			        }
			        printf("\n");
//	                        {
//	                                if(delPtr2->head->pid == message.pid)
//	                                {
//	                                        delPtr2 = delPtr2->next;
//	                                        break;
//	                                }
//	                                if(delPtr2->next->head->pid == message.pid)
//	                                {
//	                                        delPtr2->next = delPtr2->next->next;
//	                                        break;
//	                                }
//	                                delPtr2 = delPtr2->next;
//	                        }
			}
                }
		
                if(msgrcv(msgqid, &message, sizeof(message), 4, IPC_NOWAIT) > 0)
                {
                	availableResources->usedResources[message.mresReq] += message.mresNo;
			allocatedResources->usedResources[message.mresReq] -= message.mresNo;
			findProc(processList, message.pid)->used->usedResources[message.mresReq] = 0;

                        printf("OSS acknowledges child %ld is releasing resource %d which now has %d\n", (long) message.pid, message.mresReq, availableResources->usedResources[message.mresReq]);
                        if(vOpt)
				fprintf(fp, "OSS acknowledges child %ld is releasing resource %d which now has %d\n", (long) message.pid, message.mresReq, availableResources->usedResources[message.mresReq]);

			
                        message.mtype = message.pid;
                        message.mresReq = 1;
                        msgsnd(msgqid, &message, sizeof(message), 0);
                }
		
		while(blockedQueue != NULL)
		{
			resCheck = blockedQueue ->head->resReq;
			if(availableResources->usedResources[resCheck] > blockedQueue->head->used->usedResources[resCheck])
			{	
				printf("Child %ld is unblocked! Resource %d granted!\n", (long) blockedQueue->head->pid, resCheck);
				fprintf(fp, "Child %ld is unblocked! Resource %d granted!\n", (long) blockedQueue->head->pid, resCheck);
				previous->head->totalBlockedTime += blockedQueue->head->blockedBurstTime;
				printf("Add blocked burst time\n");
				blockedQueue->head->blockedBurstTime = (sim_clock->sec*1000000000 + sim_clock->nsec) - blockedQueue->head->blockedBurstTime;
				printf("Add total burst time\n");
//				previous->head->used->usedResources[resCheck]++;
				blockedPush(&processList, blockedQueue->head);
				printf("blocked push\n");
				allocatedResources->usedResources[resCheck] += blockedQueue->head->used->usedResources[resCheck];
				availableResources->usedResources[resCheck] -= blockedQueue->head->used->usedResources[resCheck];
				printf("changing resource values\n");

				message.mtype = blockedQueue->head->pid;
				message.mresReq = 1;
				msgsnd(msgqid, &message, sizeof(message), 0);
				printf("messaage sent\n");

				deleteProc(&blockedQueue, blockedQueue->head->pid);
				break;
			}
			blockedQueue = blockedQueue->next;
		}
                if(semop(semid, &p, 1) < 0)
		{
                    	perror("semop p"); 
			sigint(0);
		}	
                if (sim_clock->nsec >= 1000000000)
                {
	                sim_clock->sec++;
	                sim_clock->nsec = 0;
                }
                sim_clock->nsec += 10000;
                if(semop(semid, &v, 1) < 0)
		{
                    	perror("semop v"); 
			sigint(0);
		}

		if(tot_proc == 30)
			break;
//		printf("Chrildren still in process list: ");
//		while(processList != NULL)
//			printf("%ld, ", (long) processList->head->pid);
//		printf("\n\n");
	}
//	if(child > 0)
//		while(wait(NULL) > 0);

	printf("Main finished!\n");
	fclose(fp);

        printf("\nChildren still in process list: ");
        while(processList != NULL)
        {
                printf("%ld, ", (long) processList->head->pid);
                processList = processList->next;
        }
        printf("\n");

        printf("\nChildren still in blocked queue: ");
        while(blockedQueue != NULL)
        {
                printf("%ld, ", (long) blockedQueue->head->pid);
                blockedQueue = blockedQueue->next;
        }
        printf("\n");


	shmdt(sim_clock);
	shmctl(shmid, IPC_RMID, NULL);
        shmdt(totalResources);
        shmctl(resid, IPC_RMID, NULL);
	semctl(semid, 0, IPC_RMID), u;
        if (msgctl(msgqid, IPC_RMID, NULL) == -1)
                fprintf(stderr, "Message queue could not be deleted\n");

	kill(0, SIGQUIT);
}
