#include <iostream>
#include <list>
#include <iterator>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <vector>

#define READ_END 0
#define WRITE_END 1

#define P2K i
#define K2P i+1

#define NUM_CHILDREN(n) n
#define NUM_PIPES(m) NUM_CHILDREN(m)*2

#define WRITE(a) { const char *foo = a; write (1, foo, strlen (foo)); }

//changed seconds to 20 (1)
#define NUM_SECONDS 20

// make sure the asserts work
#undef NDEBUG
#include <assert.h>

#define EBUG
#ifdef EBUG
#   define dmess(a) cout << "in " << __FILE__ << \
    " at " << __LINE__ << " " << a << endl;

#   define dprint(a) cout << "in " << __FILE__ << \
    " at " << __LINE__ << " " << (#a) << " = " << a << endl;

#   define dprintt(a,b) cout << "in " << __FILE__ << \
    " at " << __LINE__ << " " << a << " " << (#b) << " = " \
    << b << endl
#else
#   define dprint(a)
#endif /* EBUG */

using namespace std;

enum STATE { NEW, RUNNING, WAITING, READY, TERMINATED };

int argc = 0;
int child_count = 0;
/*
** a signal handler for those signals delivered to this process, but
** not already handled.
*/
void grab (int signum) { dprint (signum); }

// c++decl> declare ISV as array 32 of pointer to function (int) returning
// void
void (*ISV[32])(int) = {
/*        00    01    02    03    04    05    06    07    08    09 */
/*  0 */ grab, grab, grab, grab, grab, grab, grab, grab, grab, grab,
/* 10 */ grab, grab, grab, grab, grab, grab, grab, grab, grab, grab,
/* 20 */ grab, grab, grab, grab, grab, grab, grab, grab, grab, grab,
/* 30 */ grab, grab
};

struct PCB
{
    STATE state;
    const char *name;   // name of the executable
    int pid;            // process id from fork();
    int ppid;           // parent process id
    int interrupts;     // number of times interrupted
    int switches;       // may be < interrupts
    int started;        // the time this process started
    vector <vector<int>> pipes;
};

/*
** an overloaded output operator that prints a PCB
*/
ostream& operator << (ostream &os, struct PCB *pcb)
{
    os << "state:        " << pcb->state << endl;
    os << "name:         " << pcb->name << endl;
    os << "pid:          " << pcb->pid << endl;
    os << "ppid:         " << pcb->ppid << endl;
    os << "interrupts:   " << pcb->interrupts << endl;
    os << "switches:     " << pcb->switches << endl;
    os << "started:      " << pcb->started << endl;
    return (os);
}

/*
** an overloaded output operator that prints a list of PCBs
*/
ostream& operator << (ostream &os, list<PCB *> which)
{
    list<PCB *>::iterator PCB_iter;
    for (PCB_iter = which.begin(); PCB_iter != which.end(); PCB_iter++)
    {
        os << (*PCB_iter);
    }
    return (os);
}

PCB *running;
PCB *idle;

// http://www.cplusplus.com/reference/list/list/
list<PCB *> new_list;
list<PCB *> processes;

int sys_time;

/*
**  send signal to process pid every interval for number of times.
*/
void send_signals (int signal, int pid, int interval, int number)
{
    dprintt ("at beginning of send_signals", getpid ());

    for (int i = 1; i <= number; i++)
    {
        sleep (interval);

        dprintt ("sending", signal);
        dprintt ("to", pid);

        if (kill (pid, signal) == -1)
        {
            perror ("kill");
            return;
        }
    }
    dmess ("at end of send_signals");
}

struct sigaction *create_handler (int signum, void (*handler)(int))
{
    struct sigaction *action = new (struct sigaction);

    action->sa_handler = handler;
/*
**  SA_NOCLDSTOP
**  If  signum  is  SIGCHLD, do not receive notification when
**  child processes stop (i.e., when child processes  receive
**  one of SIGSTOP, SIGTSTP, SIGTTIN or SIGTTOU).
*/
    if (signum == SIGCHLD)
    {
        action->sa_flags = SA_NOCLDSTOP;
    }
    else
    {
        action->sa_flags = 0;
    }

    sigemptyset (&(action->sa_mask));

    assert (sigaction (signum, action, NULL) == 0);
    return (action);
}

int eye2eh (int i, char *buf, int bufsize, int base)
{
    if (bufsize < 1) return (-1);
    buf[bufsize-1] = '\0';
    if (bufsize == 1) return (0);
    if (base < 2 || base > 16)
    {
        for (int j = bufsize-2; j >= 0; j--)
        {
            buf[j] = ' ';
        }
        return (-1);
    }

    int count = 0;
    const char *digits = "0123456789ABCDEF";
    for (int j = bufsize-2; j >= 0; j--)
    {
        if (i == 0)
        {
            buf[j] = ' ';
        }
        else
        {
            buf[j] = digits[i%base];
            i = i/base;
            count++;
        }
    }
    if (i != 0) return (-1);
    return (count);
}

PCB* choose_process ()
{
//update running PCB (3a)
        running->interrupts = running->interrupts +1;
        running->switches = running->switches+1;
        running->state = READY;
        processes.push_back(running);
//move new process (3b)
    int f;

    if(!new_list.empty()){
        running = new_list.front();
        new_list.pop_front();
//run ready processes (3c)
    if(running->name==NULL)
        running->name = "empty";
            string path = string("./") +string(running->name);

            if((f = fork()) < 0)
                perror("Error");

            else if(f == 0){
              for(int i = 0; i < NUM_PIPES(argc); i+=2){
              
                close (running->pipes[P2K][READ_END]);
                close (running->pipes[K2P][WRITE_END]);

                // assign fildes 3 and 4 to the pipe ends in the child
                dup2 (running->pipes[P2K][WRITE_END], 3);
                dup2 (running->pipes[K2P][READ_END], 4);

                execl(path.c_str(), running->name, NULL, (char*)NULL);
               
              }
            }
            else{
                running->pid = f;
                running->state = RUNNING;
            }

     processes.push_back(running);
     return running;
}

    list<PCB *>::iterator iter;
    for (iter = processes.begin(); iter != processes.end(); iter++)
    {
        if((*iter)->state == READY){
		running = *iter;
		running->state = RUNNING;
		processes.remove(*iter);
		processes.push_back(running);
		return running;
            		
	}
	
    }


    return idle;
}

void scheduler (int signum)
{
    assert (signum == SIGALRM);
    sys_time++;

    PCB* tocont = choose_process();

    dprintt ("continuing", tocont->pid);
    if (kill (tocont->pid, SIGCONT) == -1)
    {
        perror ("kill");
        return;
    }
}

void process_done (int signum)
{
    assert (signum == SIGCHLD);
    WRITE("---- entering child_done\n");

    for (;;)
    {
        int status, cpid;
        cpid = waitpid (-1, &status, WNOHANG);
        cout << running;

        if (cpid < 0)
        {
            WRITE("cpid < 0\n");
            kill (0, SIGTERM);
        }
        else if (cpid == 0)
        {
            WRITE("cpid == 0\n");
            break;
        }
        else
        {
            dprint (WEXITSTATUS (status));
            char buf[10];
            assert (eye2eh (cpid, buf, 10, 10) != -1);
            WRITE("process exited:");
            WRITE(buf);
            WRITE("\n");
            child_count++;
            if (child_count == NUM_CHILDREN(argc))
            {
                kill (0, SIGTERM);
            }
        }
      running->state = TERMINATED;
      running = idle;
    }

    WRITE("---- leaving child_done\n");

}

void process_trap (int signum)
{
    assert (signum == SIGTRAP);
    WRITE("---- entering process_trap\n");
    string names = "";

    /*
    ** poll all the pipes as we don't know which process sent the trap, nor
    ** if more than one has arrived.
    */
    for (int i = 0; i < NUM_PIPES(argc); i+=2)
    {
        char buf[1024];
        int num_read = read (running->pipes[P2K][READ_END], buf, 1023);
        if (num_read > 0)
        {
            buf[num_read] = '\0';
            WRITE("kernel read: ");
            WRITE(buf);
            WRITE("\n");

    list<PCB *>::iterator iter;
    for (iter = processes.begin(); iter != processes.end(); iter++)
    {
        names = names + ", " +(*iter)->name;
    }

      string tochar = names + string("\nSystem time: ") + to_string(sys_time);
            // respond
 //           const char *message = "from the kernel to the process";
            write (running->pipes[K2P][WRITE_END], tochar.c_str(), strlen (tochar.c_str()));
        }
    }
    WRITE("---- leaving process_trap\n");
}

/*
** stop the running process and index into the ISV to call the ISR
*/
void ISR (int signum)
{
    if (kill (running->pid, SIGSTOP) == -1)
    {
        perror ("kill");
        return;
    }
    dprintt ("stopped", running->pid);

    ISV[signum](signum);
}

/*
** set up the "hardware"
*/
void boot (int pid)
{
    ISV[SIGALRM] = scheduler;       create_handler (SIGALRM, ISR);
    ISV[SIGCHLD] = process_done;    create_handler (SIGCHLD, ISR);
    ISV[SIGTRAP] = process_trap;    create_handler (SIGTRAP, ISR);

    // start up clock interrupt
    int ret;
    if ((ret = fork ()) == 0)
    {
        // signal this process once a second for three times
        send_signals (SIGALRM, pid, 1, NUM_SECONDS);

        // once that's done, really kill everything...
        kill (0, SIGTERM);
    }

    if (ret < 0)
    {
        perror ("fork");
    }
}

void create_idle ()
{
    int idlepid;

    if ((idlepid = fork ()) == 0)
    {
        dprintt ("idle", getpid ());

        // the pause might be interrupted, so we need to
        // repeat it forever.
        for (;;)
        {
            dmess ("going to sleep");
            pause ();
            if (errno == EINTR)
            {
                dmess ("waking up");
                continue;
            }
            perror ("pause");
        }
    }
    idle = new (PCB);
    idle->state = RUNNING;
    idle->name = "IDLE";
    idle->pid = idlepid;
    idle->ppid = 0;
    idle->interrupts = 0;
    idle->switches = 0;
    idle->started = sys_time;
}

int main (int argc, char **argv)
{
    int pid = getpid();
    dprintt ("main", pid);

    sys_time = 0;

    boot (pid);

// add argv's to new_list (2)
    int nn = argc;
    while(nn > 0){
        PCB* process = new (PCB);
        process->state = READY;
        process->name = argv[nn];
        process->pid = 0;
        process->ppid = getpid();
        process->interrupts = 0;
        process->switches = 0;
        process->started = sys_time;
        process->pipes.resize(NUM_PIPES(argc), vector<int>(2, 0));

    for(int i = 0; i < NUM_PIPES(argc); i+=2){
      
        assert (pipe (process->pipes[P2K].data()) == 0);
        assert (pipe (process->pipes[K2P].data()) == 0);

        // make the read end of the kernel pipe non-blocking.
        assert (fcntl (process->pipes[P2K][READ_END], F_SETFL,
         fcntl(process->pipes[P2K][READ_END], F_GETFL) | O_NONBLOCK) == 0);
      
    }

        new_list.push_back (process);
        nn--;
    }

    // create a process to soak up cycles
    create_idle ();
    running = idle;

    cout << running;

    // we keep this process around so that the children don't die and
    // to keep the IRQs in place.
    for (;;)
    {
        pause();
        if (errno == EINTR) { continue; }
        perror ("pause");
    }
}
