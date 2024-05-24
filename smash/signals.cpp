#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlCHandler(int sig_num)
{
    Shell &smash = Shell::getInstance();
    pid_t pid = smash.fg_pid;
    cout << "smash: got ctrl-C\n";
    if (pid != 0)
    {
        if (kill(pid, SIGKILL) == -1)
        {
            perror("smash error: kill failed");
            return;
        }
        cout << "smash: process " << pid << " was killed\n";
    }
}

void alarmHandler(int sig_num)
{
  // TODO: Add your implementation
}

