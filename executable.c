#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>

#define SEC_KEY 0x1234567
#define MSG_KEY 0x2345

//Initialize the shared clock and message queue
typedef struct Clock
{
        unsigned int sec;
        unsigned int nsec;
//	pid_t shmPID;
} Clock;

struct msgbuf
{
        long mtype;
        int mtext;
	pid_t pid;
} message;


int main()
{
	int shmid, msgqid, duration, decision, decision_flag = 0;
	long long current_time = 0;
	struct Clock *sim_clock;
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
	pid = getpid();	
	//Seed the random number generator and grab a number between 0 and 50,000,000)
	srand((int)time(&t) % getpid());	
	
	//The main show
	printf("Child %ld is created\n", (long) pid);
	while(1)
	{
		if(!decision_flag)
		{
			decision = (rand() % 100);
			duration = (rand() % 5000000);
			current_time = sim_clock->sec*1000000000 + sim_clock->nsec;
			decision_flag = 1;
		}
		if(decision < 2)
		{
			printf("Child dead %ld\n", (long) pid);
			message.mtype = 2;
			message.pid = pid;
			msgsnd(msgqid, &message, sizeof(message), 0);
			exit(0);
		}
		if(decision > 2 && decision <= 50 && ((current_time + duration) <= (sim_clock->sec*1000000000 + sim_clock->nsec)))
		{
			printf("Child %ld is asking for resources!\n", (long) pid);
               	        message.mtype = 3;
               	        message.pid = pid;
               	        msgsnd(msgqid, &message, sizeof(message), 0);
			if(msgrcv(msgqid, &message, sizeof(message), pid, 0) > 0)
				decision_flag = 0;
		}
               	if(decision > 50 && decision <= 100 && ((current_time + duration) <= (sim_clock->sec*1000000000 + sim_clock->nsec)))
               	{
               	        printf("Child %ld is releasing resources!\n", (long) pid);
               	        message.mtype = 4;
               	        message.pid = pid;
               	        msgsnd(msgqid, &message, sizeof(message), 0);
			if(msgrcv(msgqid, &message, sizeof(message), pid, 0) > 0)
				decision_flag = 0;
               	}	
	}
}

