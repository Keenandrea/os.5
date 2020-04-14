/* PREPROCESSOR DIRECTIVES ============================================= */
/* ===================================================================== */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>

#include "shmem.h"
#include "queue.h"
/* END ================================================================= */


/* CONSTANTS =========================================================== */
/* ===================================================================== */
const int probthatprocterminates = 15;
const int probthatprocrequests  = 85;
const int bound = 1000000;
/* END ================================================================= */


/* GLOBAL VARIABLES ==================================================== */
/* ===================================================================== */
shmem* smseg;
int sipcid;
int tousr;
int tooss;
int pcaps = 18;
/* END ================================================================= */


/* GLOBAL MESSAGE QUEUE ================================================ */
/* ===================================================================== */
struct 
{
	long msgtype;
	char message[100];
} msg;
/* END ================================================================= */


/* FUNCTION PROTOTYPES ================================================= */
/* ===================================================================== */
void sminit();
void msginit();
void clockinc(simclock *, int, int);
/* END ================================================================= */


/* MAIN ================================================================ */
/* ===================================================================== */

int main(int argc, char *argv[])
{

    sminit();

    msginit();

	int iter;
	int proc = atoi(argv[1]);

    /* time(NULL) ensures a different val
       ue every second. (getpid() << 16))
       increases the odds that a differen
       t value computes for each process
       because process IDs typically are
       not re-used that often          */
    srand(time(NULL) ^ (getpid() << 16));

	int interim = (rand() % bound + 1);
	
	simclock worktime = {0, 0};
	simclock termtime = {0, 0};

	worktime.secs = smseg->smtime.secs;
	worktime.nans = smseg->smtime.nans;
	clockinc(&worktime, 0, interim);

	int doiterminate = (rand() % (250 * bound) + 1);
	
	termtime.secs = smseg->smtime.secs;
	termtime.nans = smseg->smtime.nans;
	clockinc(&termtime, 0, doiterminate);

	while(1)
	{
		if((smseg->smtime.secs > worktime.secs) || (smseg->smtime.secs == worktime.secs && smseg->smtime.nans >= worktime.nans))
		{
			worktime.secs = smseg->smtime.secs;
			worktime.nans = smseg->smtime.nans;
			clockinc(&worktime, 0, interim);

			if((rand() % 100) < probthatprocrequests)
			{
				strcpy(msg.message, "REQUEST");
				msg.msgtype = proc;
				msgsnd(tooss, &msg, sizeof(msg), 0);

				int requestres = (rand() % 20);
				sprintf(msg.message, "%i", requestres);
				msgsnd(tooss, &msg, sizeof(msg), 0);

				int resinstances = (rand() % (smseg->resourceclass[requestres].inventory)) + 1;

				sprintf(msg.message, "%i", resinstances);
				msgsnd(tooss, &msg, sizeof(msg), 0);
				
				while(1)
				{
					msgrcv(tousr, &msg, sizeof(msg), proc, 0);
					if(strcmp(msg.message, "GRANTED") == 0)
					{
						break;
					}

					if(strcmp(msg.message, "RESOLVE") == 0)
					{
						exit(EXIT_SUCCESS);
					}
				}
			} else {
				strcpy(msg.message, "RELEASE");
				msg.msgtype = proc;
				msgsnd(tooss, &msg, sizeof(msg), 0);

				int resourceretrieve = -1;
				int j;
				for(j = 0; j < 20; j++)
				{
					if(smseg->resourceclass[j].allocator[proc - 1] > 0)
					{
						resourceretrieve = j;
					}
				}
				sprintf(msg.message, "%i", resourceretrieve);
				msgsnd(tooss, &msg, sizeof(msg), 0);
			}
		}

		if((smseg->smtime.secs > termtime.secs) || (smseg->smtime.secs == termtime.secs && smseg->smtime.nans >= termtime.nans))
		{
			doiterminate = (rand() % (250 * bound) + 1);
			termtime.secs = smseg->smtime.secs;
			termtime.nans = smseg->smtime.nans;
			clockinc(&termtime, 0, doiterminate);

			if((rand() % 100) <= probthatprocterminates)
			{
				strcpy(msg.message, "TERMINATE");
				msg.msgtype = proc;
				msgsnd(tooss, &msg, sizeof(msg), 0);
				exit(EXIT_SUCCESS);
			}
		}		
	}
}
/* END ================================================================= */


/* ADDS TIME BASED ON SECONDS AND NANOSECONDS ========================== */
/* ===================================================================== */
void clockinc(simclock* khronos, int sec, int nan)
{
	khronos->secs = khronos->secs + sec;
	khronos->nans = khronos->nans + nan;
	while(khronos->nans >= 1000000000)
	{
		khronos->nans -= 1000000000;
		(khronos->secs)++;
	}
}
/* END ================================================================= */


/* INITIATES MESSAGES ================================================== */
/* ===================================================================== */
void msginit()
{
	key_t msgkey = ftok("msg1", 925);
	if(msgkey == -1)
	{
		perror("\noss: error: ftok failed");
		exit(EXIT_FAILURE);
	}

	tousr = msgget(msgkey, 0600 | IPC_CREAT);
	if(tousr == -1)
	{
		perror("\noss: error: failed to create");
		exit(EXIT_FAILURE);
	}

	msgkey = ftok("msg2", 825);
	if(msgkey == -1)
	{
		perror("\noss: error: ftok failed");
		exit(EXIT_FAILURE);
	}

	tooss = msgget(msgkey, 0600 | IPC_CREAT);
	if(tooss == -1)
	{
		perror("\noss: error: failed to create");
		exit(EXIT_FAILURE);
	}
}
/* END ================================================================= */


/* INITIATES SHARED MEMORY ============================================= */
/* ===================================================================== */
void sminit()
{
	key_t smkey = ftok("shmfile", 'a');
	if(smkey == -1)
	{
		perror("\noss: error: ftok failed");
		exit(EXIT_FAILURE);
	}

	sipcid = shmget(smkey, sizeof(shmem), 0600 | IPC_CREAT);
	if(sipcid == -1)
	{
		perror("\noss: error: failed to create shared memory");
		exit(EXIT_FAILURE);
	}

	smseg = (shmem*)shmat(sipcid,(void*)0, 0);

	if(smseg == (void*)-1)
	{
		perror("\noss: error: failed to attach shared memory");
		exit(EXIT_FAILURE);
	}
}
/* END ================================================================= */
