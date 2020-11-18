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

#define SEC_KEY 0x1234
#define MSG_KEY 0x2345

//Initialize the shared clock and message queue
typedef struct Clock
{
        int sec;
        long long nsec;
	pid_t shmPID;
} Clock;

struct msgbuf
{
        long mtype;
        char mtext[100];
} message;


int main()
{
	int shmid, msgqid, time_flag = 0;
	long long duration = 0, current_time = 0;
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
	duration = (rand() % 50000000);	
	
	//The main show: Check the queue, get in the critical section, and check the clock until
	//the random time duration expires
	while(1)
	{	
		msgrcv(msgqid, &message, sizeof(message), 1, IPC_NOWAIT);
		if (strcmp(message.mtext, "1") == 0)		
		{
			if (!time_flag)
			{
				current_time = sim_clock->sec*1000000000 + sim_clock->nsec;
				time_flag = 1;
			}
			if ((current_time + duration) <= (sim_clock->sec*1000000000 + sim_clock->nsec))
			{ 	
				if(!sim_clock->shmPID)
				{
					sim_clock->shmPID = getpid();
					strcpy(message.mtext, "1");
					msgsnd(msgqid, &message, sizeof(message), 0);
					break;
				}
				else
				{
					strcpy(message.mtext, "1");
					msgsnd(msgqid, &message, sizeof(message), 0);
				}	
			}
		}
	}
}

