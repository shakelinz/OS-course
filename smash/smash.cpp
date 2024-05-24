#include <iostream>
#include <unistd.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"

using namespace std;

int main(int argc, char* argv[])
{

    if(signal(SIGINT , ctrlCHandler)==SIG_ERR)
        perror("smash error: failed to set ctrl-C handler");
    
    Shell& smash = Shell::getInstance();
    while(true)
    {
        smash.printPrompt();
        string cmd_line;
        getline(cin, cmd_line);
        
        if (smash.isPipeCommand(cmd_line))
            smash.pipeCommand();

        else if (smash.isRedirectionCommand(cmd_line))
            smash.redirectionCommand(cmd_line);
        
        else
            smash.executeCommand(cmd_line.c_str());
        
        // smash.deleteArgs();
    }

    return 0;
}