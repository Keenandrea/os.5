INTRODUCTION
------------

In this module, Resource Management, a resource m
anagement module will be designed and implemented
for our Operating System Simulator oss, the other
components of which can be found here:

   https://github.com/Keenandrea?tab=repositories

This module will use the deadlock detection and r
ecovery strategy to manage resources. There is no
scheduling in this project, but shared memory wil
l be employed, so it is paramount to be cognizant
of any possible race conditions that may arise.

 * For a full description of the module, visit th
   e project page of CS4760: Operating Systems at:
   http://www.cs.umsl.edu/~sanjiv/classes/cs4760/

 * To submit bug reports and feature suggestions o
   r track changes visit:
   https://github.com/Keenandrea?tab=repositories


REQUIREMENTS
------------

This module requires the following:

 * Hoare Linux (http://www.umsl.edu/technology/tsc/)
 * UMSL ID (https://www.umsl.edu/degrees/index.html)


INSTALLATION
------------

 * Install as you would normally install a distrib
   uted C module. To compile and execute, you must
   have the build-essential packages properly inst
   alled on your system. 


COMPILATION
-----------

To compile, open a new CLI window, change the the
directory nesting your module. Type:

 * make


EXECUTION
---------
Find the executable named master located in the d
irectory in which you compiled the module. Once y
ou have found it, run the following command:

	./oss

That will get the ball rolling. Check the ouput f
ile for details on execution. The output file is:
	
	log.txt

To see the contents of the most recent run, in yo
ur terminal, type:

	cat log.txt

In addition, there is a help option that you can
specify by running the command:

	./oss -h 

And another option to set the mode to verbose, w
hich will indicate in the log file every time os
s gives someone a requested resource or when mas
ter sees that a user has finished with a resourc
e. It will also log the time when deadlock is de
tected, and how the system removed deadlock. Tha
t is, which processes were terminated. In additi
on, at every 20 granted requests, verbose mode w
ill output a table showing the current resources
allocated to each process. To initiate verbose m
ode, type the following command at execution:
	
	./oss -v


MAINTAINERS
-----------

Current maintainers:
 * Keenan Andrea - https://github.com/Keenandrea

This project has been sponsored by:
 * UMSL Mathematics and Computer Science 
   Specialized in bringing young minds to graduat
   ion with the skills needed. 


DOCUMENTATION OF DEADLOCK RESOLUTION
------------- -- -------- ----------
The simulation will check if any user process has
gotten itself in deadlock when requesting resourc
es. If a process has indeed gotten itself in dead
lock, the deadlock processes will terminate, one 
by one, until the deadlock is resolved.

























































