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
	int shmid, msgqid, decision, decision_flag = 0;
//	long long current_time = 0;
	struct Clock *sim_clock;
	time_t t;
	
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

	//Seed the random number generator and grab a number between 0 and 50,000,000)
	srand((int)time(&t) % getpid());	
	
	//The main show: Check the queue, get in the critical section, and check the clock until
	//the random time duration expires
	while(1)
	{	
		if(!decision_flag)
		{
			decision = (rand() % 100);
			printf("Child %ld's decision is %d\n", (long) getpid(), decision);
			decision_flag = 1;
		}
		if(decision < 2)
		{
			if(msgrcv(msgqid, &message, sizeof(message), 1, IPC_NOWAIT) > 0)
			{
				printf("Child dead %ld\n", (long) getpid());
				message.mtype = 2;
				message.pid = getpid();
				msgsnd(msgqid, &message, sizeof(message), 0);
				exit(0);
			}
		}
		if(decision > 2 && decision <= 50)
		{
			if(msgrcv(msgqid, &message, sizeof(message), 1, IPC_NOWAIT) > 0)
			{
				printf("I am asking for resources!\n");
                                message.mtype = 3;
                                message.pid = getpid();
                                msgsnd(msgqid, &message, sizeof(message), 0);
				decision_flag = 0;
			}
		}
                if(decision > 50 && decision <= 100)
                {
                        if(msgrcv(msgqid, &message, sizeof(message), 1, IPC_NOWAIT) > 0)
                        {
                                printf("I am releasing resources!\n");
                                message.mtype = 4;
                                message.pid = getpid();
                                msgsnd(msgqid, &message, sizeof(message), 0);
				decision_flag = 0;
                        }
                }	
//		printf("Child here %ld\n", (long) getpid());
//		sleep(1);
	}
}

