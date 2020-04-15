/* PREPROCESSOR DIRECTIVES ============================================= */
/* ===================================================================== */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <sys/time.h>
#include <sys/types.h>
#include <sys/msg.h>

#include "queue.h"
#include "shmem.h"
/* END ================================================================= */


/* CONSTANTS =========================================================== */
/* ===================================================================== */
const int maxtimebetweennewprocsnans = 500000;
const int maxtimebetweennewprocssecs = 1;
const int basetimequantum = 10;
const int maxrandinterval = 1000;
const int weightedrealtime = 20;
/* ===================================================================== */


/* GLOBAL VARIABLES ==================================================== */
/* ===================================================================== */
struct sigaction satime;
struct sigaction sactrl;
struct Queue *waitinglist;
shmem *smseg;
FILE* outlog;
int vbose = 0;
int sipcid;
int tousr;
int tooss;
int pcap = 18;
int plist[18];
pid_t pids[18];
int tableprinter = 1;
int eventualprocesstermination = 0;
int grantedresource = 0;
int deadlockdetectcount = 0;
int deadlocktermination = 0;
int pcount = 0;
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
void optset(int, char **);
void helpme();
static int satimer();
void killctrl(int, siginfo_t *, void *);
void killtime(int, siginfo_t *, void *);

void shminit();
void msginit();
void resinit();
void ticinit();

void moppingup();
void manager(int);

void clockinc(simclock *, int, int);
int timecomparison(simclock *, simclock *);

void overlay(int);
int findaseat();
int resourceallocator(int, int);
void resourcedeallocator(int, int);
void getresource(int, int, int);
/* END ================================================================= */


/* MAIN ================================================================ */
/* ===================================================================== */
int main(int argc, char *argv[])
{
    optset(argc, argv);

	satime.sa_sigaction = killtime;
	sigemptyset(&satime.sa_mask);
	satime.sa_flags = 0;
	sigaction(SIGALRM, &satime, NULL);

    sactrl.sa_sigaction = killctrl;
	sigemptyset(&sactrl.sa_mask);
	sactrl.sa_flags = 0;
	sigaction(SIGINT, &sactrl, NULL);

	outlog = fopen("log.txt", "w");
	if(outlog == NULL)
	{
		perror("\noss: error: failed to open output file");
		exit(EXIT_FAILURE);
	}

	srand(time(NULL));

	shminit();
	msginit();
	resinit();
	ticinit();

	manager(vbose);
	
	moppingup();

return 0;
}
/* END ================================================================= */


/* PROCESS SCHEDULER =================================================== */
/* ===================================================================== */
void manager(int vbose)
{
	int status;
	int msglen;
	int temp;
    int ecount = 0;
	int acount = 0;
	int lcount = 0;
	int iterat = 0;
	pid_t cpid;

	simclock forktime = {0, 0};
	simclock deadlock = {1, 0};

	waitinglist = queueinit(200);

	/* deploy random time between 1 and 500 milliseconds of logical
	   clock, which is, at this point, initiated to { 0, 0 }     */
	int nextfork = (rand() % (500000000 - 1000000 + 1)) + 1000000;
	clockinc(&forktime, 0, nextfork);

	if(vbose == 1)
	{
		printf("\n[oss]: running simulation in verbose mode\n[oss]: ctrl-c to terminate\n");
	} else {
		printf("\n[oss]: running simulation\n[oss]: ctrl-c to terminate\n");	
	}

	while(1)
	{
		/* advance clock by 10 milliseconds,
		   or 0.01 seconds at the beginning
		   of each loop to simulate time  */
		clockinc(&(smseg->smtime), 0, 10000);

		if(acount < pcap && ((smseg->smtime.secs > forktime.secs) || (smseg->smtime.secs == forktime.secs && smseg->smtime.nans >= forktime.nans)))
		{
			/* set previous user process fork t
			ime to current logical clock  */
			forktime.secs = smseg->smtime.secs;
			forktime.nans = smseg->smtime.nans;
			/* then add random time between 1 and 500 milliseconds of
			current logical clock to set chronomatic deployment va
			riable for upcoming random time user process fork   */
			nextfork = (rand() % (500000000 - 1000000 + 1)) + 1000000;
			clockinc(&forktime, 0, nextfork);
			
			int seat = findaseat() + 1;
			if(seat - 1 > -1)
			{
				pids[seat - 1] = fork();
				if(pids[seat - 1] == 0)
				{
					overlay(seat);
				}
				acount++;
				pcount = acount;
					
				if(lcount < 100000 && vbose == 1)
				{
					lcount++;
					fprintf(outlog, "\n[oss]: [spawn process]     -> [pid: %i] [time: %is:%ins]", cpid, smseg->smtime.secs, smseg->smtime.nans);
				}
			}
		}

		if(msgrcv(tooss, &msg, sizeof(msg), 0, IPC_NOWAIT) > -1)
		{
			int proc = msg.msgtype;

			if(strcmp(msg.message, "TERMINATE") == 0)
			{
				while(waitpid(pids[proc - 1], NULL, 0) > 0);

				eventualprocesstermination++;

				for(iterat = 0; iterat < 20; iterat++)
				{
					if(smseg->resourceclass[iterat].shareable == 0)
					{
						if(smseg->resourceclass[iterat].allocator[proc - 1] > 0)
						{
							smseg->resourceclass[iterat].available += smseg->resourceclass[iterat].allocator[proc - 1];
						}
					}
					smseg->resourceclass[iterat].allocator[proc - 1] = 0;
					smseg->resourceclass[iterat].request[proc - 1] = 0;
				}
				
				acount--;
				ecount++;
				plist[proc - 1] = 0;

				if(lcount < 100000 && vbose == 1)
				{
					lcount++;
					fprintf(outlog, "\n[oss]: [detected process]  -> [p%i] terminating normally at [time: %is:%ins]", proc, smseg->smtime.secs, smseg->smtime.nans);
				}
			}



			else if(strcmp(msg.message, "RELEASE") == 0)
			{
				msgrcv(tooss, &msg, sizeof(msg), proc, 0);
				int resrelease = atoi(msg.message);

				if(resrelease > -1)
				{
					resourcedeallocator(resrelease, proc);
					if(vbose == 1)
					{	
						if(lcount < 100000 && vbose == 1)
						{
							lcount++;
							fprintf(outlog, "\n[oss]: [detected process]  -> [p%i] releasing [r%i] at [time: %is:%ins]", proc, resrelease, smseg->smtime.secs, smseg->smtime.nans);
						}
					}
				}
			}



			else if(strcmp(msg.message, "REQUEST") == 0)
			{
				msgrcv(tooss, &msg, sizeof(msg), proc, 0);
				int resrequest = atoi(msg.message);

				if(vbose == 1)
				{
					if(lcount < 100000 && vbose == 1)
					{
						lcount++;
						fprintf(outlog, "\n[oss]: [detected process]  -> [p%i] requesting [r%i] at [time: %is:%ins]", proc, resrequest, smseg->smtime.secs, smseg->smtime.nans);
					}
				}
				msgrcv(tooss, &msg, sizeof(msg), proc, 0);
				int rcount = atoi(msg.message);

				smseg->resourceclass[resrequest].request[proc -1] = rcount;

				if(resourceallocator(resrequest, proc) == 1)
				{
					grantedresource++;
					if(grantedresource % 20 == 0)
					{
						tableprinter = 1; 
					}	

					strcpy(msg.message, "GRANTED");
					msg.msgtype = proc;
					msgsnd(tousr, &msg, sizeof(msg), IPC_NOWAIT);
					
					if(lcount < 100000 && vbose == 1)
					{
						lcount++;
						fprintf(outlog, "\n[oss]: [detected process]  -> [p%i] requesting [r%i] at [time: %is:%ins]", proc, resrequest, smseg->smtime.secs, smseg->smtime.nans);
					}
				} else {
					if(lcount < 100000 && vbose == 1)
					{
						lcount++;
						fprintf(outlog, "\n[oss]: [detected process]  -> [p%i] requesting [r%i] at [time: %is:%ins]", proc, resrequest, smseg->smtime.secs, smseg->smtime.nans);
					}
					enqueue(waitinglist, proc);
				}
			}
		}

		int m, n = 0;
		if(isempty(waitinglist) == 0)
		{
			int s = getsize(waitinglist);

			while(m < s)
			{
				int proc = dequeue(waitinglist);
				int resrequest;

				for(n = 0; n < 20; n++)
				{
					if(smseg->resourceclass[n].request[proc - 1] > 0)
					{
						resrequest = n;
					}
				}

				if(resourceallocator(resrequest, proc) == 1)
				{
					grantedresource++;
					if(grantedresource % 20 == 0)
					{
						tableprinter = 1;
					}
				
					if(lcount < 100000 && vbose == 1)
					{
						lcount++;
						fprintf(outlog, "\n[oss]: [detected process]  -> [p%i] requesting [r%i] at [time: %is:%ins]", proc, resrequest, smseg->smtime.secs, smseg->smtime.nans);
					}

					strcpy(msg.message, "GRANTED");
					msg.msgtype = proc;
					msgsnd(tousr, &msg, sizeof(msg), IPC_NOWAIT);	
				} else {
					enqueue(waitinglist, proc);
				}
				m++;
			} 
		}

		if(smseg->smtime.secs == deadlock.secs)
		{
			deadlock.secs++;
			int i, j;
			int lock;
			deadlockdetectcount++;

			if(isempty(waitinglist) == 0)
			{
				lock = 1;
					
				if(lcount < 100000)
				{
					lcount++;
					fprintf(outlog, "\n[oss]: [detected deadlock] at [time: %is:%ins]", smseg->smtime.secs, smseg->smtime.nans);
				}
			} else {
				lock = 0;
			}

			if(lock == 1)
			{
				while(1)
				{
					lock = 0;
					int termpid = dequeue(waitinglist);
					msg.msgtype = termpid;
					
					strcpy(msg.message, "RESOLVE");
					msg.msgtype = termpid;

					msgsnd(tousr, &msg, sizeof(msg), IPC_NOWAIT);
					while(waitpid(plist[termpid - 1], NULL, 0) > 0);
					deadlocktermination++;

					if(lcount < 100000)
					{
						lcount++;
						fprintf(outlog, "\n[oss]: [resolved process]-> [p%i] in deadlock at [time: %is:%ins]", termpid, smseg->smtime.secs, smseg->smtime.nans);
					}

					if(vbose == 1)
					{
						if(lcount < 100000 && vbose == 1)
						{
							lcount++;
							fprintf(outlog, "\n\n\tReleased Resources\n\t-------- ---------\n");
						}
					}

					for(iterat = 0; iterat < 20; iterat++)
					{
						if(smseg->resourceclass[iterat].shareable == 0)
						{
							if(smseg->resourceclass[iterat].allocator[termpid - 1] > 0)
							{
								smseg->resourceclass[iterat].available += smseg->resourceclass[iterat].allocator[termpid - 1];
								if(vbose == 1)
								{
									if(lcount < 100000 && vbose == 1)
									{
										lcount++;
										fprintf(outlog, "\tr%i ", iterat);
									}
								}
							}
						}
						smseg->resourceclass[iterat].allocator[termpid - 1] = 0;
						smseg->resourceclass[iterat].request[termpid - 1] = 0;
					}

					if(lcount < 100000 && vbose == 1)
					{
						fprintf(outlog, "\n");
					}

					acount--;
					ecount++;
					plist[termpid - 1] = 0;

					int h = 0;
					while(h < getsize(waitinglist))
					{
						int proctemp = dequeue(waitinglist);
						getresource(proctemp, vbose, lcount);
						h++;
					}

					if(isempty(waitinglist) != 0)
					{
						break;
					}
				}
				if(vbose == 1)
				{
					if(lcount < 100000 && vbose == 1)
					{
						lcount++;
						fprintf(outlog, "\nsystem has removed itself from deadlock\n");
					}
				}
			}
		}

		if((grantedresource % 20 == 0 && grantedresource != 0) && tableprinter == 1)
		{
			int x = 0;
			int y = 0;

			if(vbose == 1)
			{
				if(lcount < 100000 && vbose == 1)
				{
					lcount++;
					fprintf(outlog, "\n\n\tResource Graph\n\t-------- -----\n");
				}
			}
			fprintf(outlog, "\n\n");
			fprintf(outlog, "     [r1]  [r2]  [r3]  [r4]  [r5]  [r6]  [r7]  [r8]  [r9]  [r10] [r11] [r12] [r13] [r14] [r15] [r16] [r17] [r18] [r19] [r20]\n");	
			for(x = 0; x < 18; x++)
			{
				if(plist[x] == 1)
				{
					lcount++;
					fprintf(outlog, "[p%i]", x + 1);
						
						if(x <= 8)
						{
							fprintf(outlog, " ");
						}

						for(y = 0; y < 20; y++)
						{
							fprintf(outlog, "%i     ", smseg->resourceclass[y].allocator[x]);
						}
						fprintf(outlog, "\n");	
				}
			}
			fprintf(outlog, "\n");
			lcount = lcount + 18;
			tableprinter = 0;
		}
	}

	pid_t waitpid;
	while((waitpid = wait(&status)) > 0);

	fprintf(outlog, "\n\n\tTracked Statistics\n\t------- ----------\n");
	fprintf(outlog, "\tNumber of requests granted: [%i]\n", grantedresource);
	fprintf(outlog, "\tNumber of deadlock detection runs: [%i]\n", deadlockdetectcount);
	fprintf(outlog, "\tTotal processes terminated by deadlock: [%i]\n", deadlocktermination);
	fprintf(outlog, "\tTotal processes terminated eventually: [%i]\n", eventualprocesstermination);
	float deadlocktermper = deadlockdetectcount / deadlocktermination;
	fprintf(outlog, "\tPercentage of processes terminated in deadlock: [%f percent]\n", deadlocktermper);
}
/* END ================================================================= */


/* GETS A RESOURCE ===================================================== */
/* ===================================================================== */
void getresource(int proc, int vbose, int lcount)
{
	int resrequest;
	int i;
	for(i = 0; i < 20; i++)
	{
		if(smseg->resourceclass[i].request[proc - 1] > 0)
		{
			resrequest = i;
		}
	}

	if(resourceallocator(resrequest, proc) == 1)
	{
		if(vbose == 1)
		{
			if(lcount < 100000 && vbose == 1)
			{
				lcount++;
				fprintf(outlog, "\n[oss]: [detected resource]  -> [r%i] granted to [p%i] at [time: %is:%ins]", resrequest, proc, smseg->smtime.secs, smseg->smtime.nans);
			}
		}

		grantedresource++;
		if(grantedresource % 20 == 0)
		{
			tableprinter = 1;
		}

		strcpy(msg.message, "granted");
		msg.msgtype = proc;
		msgsnd(tousr, &msg, sizeof(msg), IPC_NOWAIT);
	} else {
		enqueue(waitinglist, proc);
	}
}
/* END ================================================================= */


/* IMITATES ALLOCATOR ================================================== */
/* ===================================================================== */
void resourcedeallocator(int resid, int proc)
{
	if(smseg->resourceclass[resid].shareable == 0)
	{
		smseg->resourceclass[resid].available += smseg->resourceclass[resid].allocator[proc - 1];
	}
	smseg->resourceclass[resid].allocator[proc - 1] = 0;
}
/* END ================================================================= */


/* IMITATES ALLOCATOR ================================================== */
/* ===================================================================== */
int resourceallocator(int resid, int proc)
{
	while((smseg->resourceclass[resid].request[proc - 1] > 0 && smseg->resourceclass[resid].available > 0))
	{
		if(smseg->resourceclass[resid].shareable == 0)
		{
			(smseg->resourceclass[resid].request[proc - 1])--;
			(smseg->resourceclass[resid].allocator[proc - 1])++;
			(smseg->resourceclass[resid].available)--;
		} else {
			smseg->resourceclass[resid].request[proc - 1] = 0;
			break;
		}
	}

	if(smseg->resourceclass[resid].request[proc - 1] > 0)
	{
		return -1;
	} else {
		return 1;
	}
}
/* END ================================================================= */


/* INITIALIZE PCB FOR PROCESS ========================================== */
/* ===================================================================== */
int findaseat()
{
	int searcher;
	for(searcher = 0; searcher < pcap; searcher++)
	{
		if(plist[searcher] == 0)
		{
			plist[searcher] = 1;
			return searcher;
		}
	}
	return -1;
}
/* END ================================================================= */


/* BITMADE JANITOR ===================================================== */
/* ===================================================================== */
void moppingup()
{
	//shmdt(smseg);
	shmctl(sipcid, IPC_RMID, NULL);
	msgctl(tooss, IPC_RMID, NULL);
	msgctl(tousr, IPC_RMID, NULL);
	fclose(outlog);
}
/* END ================================================================= */


/* OVERLAYS PROGRAM IMAGE WITH EXECV =================================== */
/* ===================================================================== */
void overlay(int id)
{
	char proc[20]; 
					
	sprintf(proc, "%i", id);	
				
	char* fargs[] = {"./usr", proc, NULL};
	execv(fargs[0], fargs);

	/* oss will not reach here unerred */	
	perror("\noss: error: exec failure");
	exit(EXIT_FAILURE);	
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


/* INITIATES CLOCK ===================================================== */
/* ===================================================================== */
void ticinit()
{
	smseg->smtime.secs = 0;
	smseg->smtime.nans = 0;
}
/* END ================================================================= */


/* INITIATES RESOURCE DATA STRUCTURES ================================== */
/* ===================================================================== */
void resinit()
{
	int m;
	int n;
	for(n = 0; n < 20; n++)
	{
		smseg->resourceclass[n].shareable = 0;
		int initinstances = (rand() % 10) + 1;

		smseg->resourceclass[n].inventory = initinstances;
		smseg->resourceclass[n].available = initinstances;

		for(m = 0; m <= 18; m++)
		{
			smseg->resourceclass[n].request[m] = 0;
			smseg->resourceclass[n].release[m] = 0;
			smseg->resourceclass[n].allocator[m] = 0;
		}
	}

	int initshareables = (rand() % 4) + 2;
	for(n = 0; n < initshareables; n++)
	{
		int dropzone = (rand() % 20);	
		smseg->resourceclass[dropzone].shareable = 0;
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
void shminit()
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


/* HANDLES SIGNALS ===================================================== */
/* ===================================================================== */
void killtime(int sig, siginfo_t *sainfo, void *ptr)
{
	char msgtime[] = "\n[oss]: exit: simulation terminated after 10s run.\n\nrefer to log.txt for results.\n\n";
	int msglentime = sizeof(msgtime);

	write(STDERR_FILENO, msgtime, msglentime);

	// int i;
	// for(i = 0; i < pcap; i++)
	// {
	// 	if(pids[i] != 0)
	// 	{
	// 		if(kill(pids[i], SIGTERM) == -1)
	// 		{
	// 			perror("\noss: error: ");			
	// 		}
	// 	}
	// }


	fprintf(outlog, "\n\n\tTracked Statistics\n\t------- ----------\n");
	fprintf(outlog, "\tNumber of requests granted: [%i]\n", grantedresource);
	fprintf(outlog, "\tNumber of deadlock detection runs: [%i]\n", deadlockdetectcount);
	fprintf(outlog, "\tTotal processes terminated by deadlock: [%i]\n", deadlocktermination);
	fprintf(outlog, "\tTotal processes terminated eventually: [%i]\n", eventualprocesstermination);
	float deadlocktermper = deadlockdetectcount / deadlocktermination;
	fprintf(outlog, "\tPercentage of processes terminated in deadlock: [%f percent]\n", deadlocktermper);

	fclose(outlog);
	//shmdt(smseg);
	shmctl(sipcid, IPC_RMID, NULL);
	msgctl(tooss, IPC_RMID, NULL);
	msgctl(tousr, IPC_RMID, NULL);

	kill(getpid(), SIGTERM);

	exit(EXIT_SUCCESS);			
}
/* END ================================================================= */


/* HANDLES CTRL-C SIGNAL =============================================== */
/* ===================================================================== */
void killctrl(int sig, siginfo_t *sainfo, void *ptr)
{
	char msgctrl[] = "\n[oss]: exit: received ctrl-c interrupt signal\n\nrefer to log.txt for results.\n\n";
	int msglenctrl = sizeof(msgctrl);

	write(STDERR_FILENO, msgctrl, msglenctrl);

	// int i;
	// for(i = 0; i < pcap; i++)
	// {
	// 	if(pids[i] != 0)
	// 	{
	// 		if(kill(pids[i], SIGTERM) == -1)
	// 		{
	// 			perror("\noss: error: ");
	// 		}
	// 	}
	// }

	fprintf(outlog, "\n\n\tTracked Statistics\n\t------- ----------\n");
	fprintf(outlog, "\tNumber of requests granted: [%i]\n", grantedresource);
	fprintf(outlog, "\tNumber of deadlock detection runs: [%i]\n", deadlockdetectcount);
	fprintf(outlog, "\tTotal processes terminated by deadlock: [%i]\n", deadlocktermination);
	fprintf(outlog, "\tTotal processes terminated eventually: [%i]\n", eventualprocesstermination);
	double deadlocktermper = deadlockdetectcount / deadlocktermination;
	fprintf(outlog, "\tPercentage of processes terminated in deadlock: [%f percent]\n", deadlocktermper);

	fclose(outlog);
	//shmdt(smseg);
	shmctl(sipcid, IPC_RMID, NULL);
	msgctl(tooss, IPC_RMID, NULL);
	msgctl(tousr, IPC_RMID, NULL);

	kill(getpid(), SIGTERM);

	exit(EXIT_SUCCESS);			
}
/* END ================================================================= */


/* SETS TIMER ========================================================== */
/* ===================================================================== */
static int satimer()
{
	struct itimerval t;
	t.it_value.tv_sec = 10;
	t.it_value.tv_usec = 0;
	t.it_interval.tv_sec = 0;
	t.it_interval.tv_usec = 0;
	
	return(setitimer(ITIMER_REAL, &t, NULL));
}
/* END ================================================================= */


/* SETS OPTIONS ======================================================== */
/* ===================================================================== */
void optset(int argc, char *argv[])
{
	int choice;
	while((choice = getopt(argc, argv, "hv")) != -1)
	{
		switch(choice)
		{
			case 'h':
				helpme();
				exit(EXIT_SUCCESS);
			case 'v':
				vbose = 1;
				break;
			case '?':
				fprintf(stderr, "\noss: error: invalid argument\n");
				exit(EXIT_FAILURE);				
		}
	}
}
/* END ================================================================= */


/* SETS HELP ========================================================== */
/* ===================================================================== */
void helpme()
{
	printf("\n|HELP|MENU|\n\n");
    printf("\t-h : display help menu\n");
	printf("\t-v : turn on verbose mode\n");
}
/* END ================================================================= */
