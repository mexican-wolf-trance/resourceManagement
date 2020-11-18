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
#include <signal.h>
#include <time.h>

#define BUFSIZE 1024
#define SEC_KEY 0x1234
#define MSG_KEY 0x2345

//Initialize the message and clock structs!
struct msgbuf
{
	long mtype;
	int mtext;
	pid_t pid;
} message;

typedef struct Clock
{
	int sec;
	long long nsec;
//	pid_t shmPID;
} Clock;

//I intitalize the ids and the clock pointer here so my signal handler can use them
int shmid, msgqid;
struct Clock *sim_clock;
struct Clock *new_proc_clock;

//set next process fork time
int newProcTime()
{
	new_proc_clock->nsec = sim_clock->nsec + (rand() % 500) * 100000;
	new_proc_clock->sec = sim_clock->sec;
}
//check if time for a new process to begin
int checkProcTime()
{
	if((new_proc_clock->nsec + new_proc_clock-sec * 1000000000) >= (sim_cloc->nsec + sim_clock->sec * 1000000000))
	{
		newProcTime();
		return 1;
	}
}

//The signal handler! Couldn't figure out how to close the file stream before getting signal though
void sigint(int sig)
{
	printf("\nTime ended: %d seconds, %lli nanoseconds\n", sim_clock->sec, sim_clock->nsec);
	if (msgctl(msgqid, IPC_RMID, 0) < 0)
	{
		perror("msgctl");
		exit(0);
	}

        shmdt(sim_clock);
        shmctl(shmid, IPC_RMID, NULL);

	write(1, "Interrupt!\n", 12);
	write(1, "Now killing the kiddos\n", 23);
	kill(0, SIGQUIT);
	exit(0);
}


//The main function
int main (int argc, char **argv)
{
	//The CTRL C catch
	signal(SIGINT, sigint);

	int option, max_child = 17, max_time = 20, counter = 0, tot_proc = 0, vOpt;;
	char file[32], *exec[] = {"./user", NULL};
	pid_t child = 0;
	FILE *fp;

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
	
	//Message queue key
	msgqid = msgget(MSG_KEY, 0644 | IPC_CREAT);
        if (msgqid == -1)
        {
                perror("msgqid get failed");
                return 1;
        }

//	sim_clock->shmPID = 0;
	//Parse the argmuents!
	if (argc > 2)
	{
		printf("Invalid usage! Check the readme\nUsage: ./oss\n");
		printf("Use the -v option for a more verbose log file") 
		return 0;
	}
	
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
	//Begin the message queue by putting a message in it
//	message.mtype = 1;
//	strcpy(message.mtext,"1");
//	msgsnd(msgqid, &message, sizeof(message), 0);	
	
	//Begin the alarm. Goes off after the -t amount of time expires
	alarm(max_time);
	signal(SIGALRM, sigint);

	//Open the log file
	if ((fp = fopen("log.out", "w")) == 0)
	{
               perror("log.out");
               sigint(0);
        }
	
	//Now we start the main show
	while(1)
	{
		if(counter <= max_child && checkProcTime())
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
			totalProc++;
		}
//		msgrcv(msgqid, &message, sizeof(message), 1, IPC_NOWAIT);
                if (sim_clock->nsec >= 1000000000)
                {
	                sim_clock->sec++;
	                sim_clock->nsec = 0;
                }
                sim_clock->nsec += 1000;
	}

        if (msgctl(msgqid, IPC_RMID, 0) < 0)
        {
                perror("msgctl");
                return 1;
        }
	shmdt(sim_clock);
	shmctl(shmid, IPC_RMID, NULL);

	kill(0, SIGQUIT);
}
