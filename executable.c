#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <signal.h>
#include <time.h>

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

//Initialize the shared clock, total resource tracker, and message queue
typedef struct Clock
{
        unsigned int sec;
        unsigned int nsec;
} Clock;

struct msgbuf
{
        long mtype;
        int mresReq;
	int mresNo;
	pid_t pid;
} message;

typedef struct Resources
{
        int usedResources[20];
} Resources;

int main()
{
	int i, shmid, msgqid, resid, semid, res_no, duration, decision, request_flag = 0, decision_flag = 0, dead_flag = 0, resChoice, requested[20];
	long long current_time = 0, time_check = 0;
	struct Clock *sim_clock;
	struct Resources *totalResources, *allocatedRes;
	time_t t;
	pid_t pid;
	
	//Get the keys to the queue and shared memory	
        msgqid = msgget(MSG_KEY, 0644 | IPC_CREAT);
        if (msgqid == -1)
        {
                perror("msgqid get failed");
                return 1;
        }


	shmid = shmget(SEC_KEY, sizeof(Clock), 0644 | IPC_CREAT);
        if (shmid == -1)
        {
                perror("shmid get failed");
                return 1;
        }
	
	//Access the shared clock
        sim_clock = (Clock *) shmat(shmid, NULL, 0);
        if (sim_clock == (void *) -1)
        {
                perror("clock get failed");
                return 1;
        }

	//Access shared resources totals
	resid = shmget(RES_KEY, sizeof(Resources), 0644 | IPC_CREAT);
        if(resid == -1)
        {
                perror("resid get failed");
                return 1;
        }  
        totalResources = (Resources *) shmat(resid, NULL, 0);
        if(totalResources == (void *)-1)
        {
                perror("total resources get failed");
                return 1;
        }
 
        semid = semget(SEM_KEY, 1, 0644 | IPC_CREAT);
        if(semid == -1)
        {
                perror("semid get failed");
                return 1;
        }

	allocatedRes = malloc(sizeof(Resources));
	for(i = 0; i < 20; i++)
		allocatedRes->usedResources[i] = 0;

	pid = getpid();	
	//Seed the random number generator and grab a number between 0 and 50,000,000)
	srand((int)time(&t) % pid);
	
	printf("Child %ld requests:\n", (long) pid);
	for(i = 0; i < 20; i++)
	{
		requested[i] = (rand() % totalResources->usedResources[i]);
		printf("%d: %d ", i, requested[i]);
	}
	printf("\n");
	
	//The main show
//	printf("Child %ld is created\n", (long) pid);
	while(1)
	{
		if(semop(semid, &p, 1) < 0)
		{
                    	perror("semop p"); 
			exit(EXIT_FAILURE);
		}
		time_check = sim_clock->sec*1000000000 + sim_clock->nsec;
		if(!decision_flag)
		{
			resChoice = (rand() % 20);
			decision = (rand() % 100);
			duration = (rand() % 2500000);
			current_time = sim_clock->sec*1000000000 + sim_clock->nsec;
			if(current_time > 1000000000)
				dead_flag = 1;
			decision_flag = 1;
		}
                if(semop(semid, &v, 1) < 0)
		{
                    	perror("semop v"); 
			exit(EXIT_FAILURE);
		}
		if(dead_flag && decision < 2)
		{
			printf("Child dead %ld\nResources used ", (long) pid);
			for(i = 0; i < 20; i++)
				printf("%d: %d ", i, allocatedRes->usedResources[i]);
			printf("\n");
			message.mtype = 2;
			message.pid = pid;
			msgsnd(msgqid, &message, sizeof(message), 0);
			exit(0);
		}
		if(decision > 2 && decision <= 70 && ((current_time + duration) <= time_check))
		{
			while(1)
			{
				if(requested[resChoice] != allocatedRes->usedResources[resChoice] && requested[resChoice] > 0)
				{
					res_no = (rand() % requested[i]) + 1;
					printf("Child %ld is requesting %d resource!\n", (long) pid, resChoice);
					message.mtype = 3;
               	        		message.pid = pid;
					message.mresReq = resChoice;
					message.mresNo = res_no;
               	        		if((msgsnd(msgqid, &message, sizeof(message), 0) < 0))
					{
						perror("Message send failed\n");
					}
					if(msgrcv(msgqid, &message, sizeof(message), pid, 0) > 0)
					{
						allocatedRes->usedResources[resChoice] += res_no;
						decision_flag = 0;
						request_flag = 1;
					}
				}
				if(requested[resChoice] == 0)
					resChoice = (rand() % 20);
				if(!decision_flag)
					break;
			}
		}
               	if(request_flag && decision > 70 && decision <= 100 && ((current_time + duration) <= time_check))
               	{
			while(1)
			{
				if(requested[resChoice] == 0)
				{
					decision_flag = 0;
					break;
				}	
				i = 0;
				while(i < 20)
				{
					if((i == resChoice) && (allocatedRes->usedResources[resChoice] > 0))
					{
               	        			printf("Child %ld is releasing resource %d!\n", (long) pid, resChoice);
               	        			message.mtype = 4;
               	        			message.pid = pid;
						message.mresReq = resChoice;
						message.mresNo = allocatedRes->usedResources[resChoice];
               	        			msgsnd(msgqid, &message, sizeof(message), 0);
						if(msgrcv(msgqid, &message, sizeof(message), pid, 0) > 0)
						{
							allocatedRes->usedResources[resChoice] -= 1;
							decision_flag = 0;
							break;
						}
					}
					i++;
				}
				resChoice = (rand() % 20);
				if(!decision_flag)
					break;
			}
               	}	
	}
}

