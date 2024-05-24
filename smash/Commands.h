#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>

using namespace std;

const int MAX_ARGS = 21;
const int MAX_PATH = 81;

class JobsList 
{
  public:
    class Job
    {
      public:
        pid_t  pid;
        int    id;
        bool   running;
        string cmd;

        Job(pid_t pid, string cmd);
    };

    vector<Job*> list;

    void add(pid_t pid, string cmd);
    void remove(int id);
    void removeFinishedJobs();
    void printJobsList();
    Job* getJobById(int id);

//   JobsList();
//   ~JobsList();
//   void killAllJobs();
//   Job * getLastJob(int* lastJobId);
//   Job *getLastStoppedJob(int *jobId);
};

class Shell
{
  private:
    Shell();
    
  public:
    string  prompt;
    pid_t   pid;
    char    previous[MAX_PATH];
    string  command;
    char*   argv[MAX_ARGS];
    int     argc;
    bool    background;
    pid_t   fg_pid;
    
    // redirection stuff:
    bool    complex;
    bool    append;
    string  output_file;
    int     stdout_index;
    int     fd;

    // pipe stuff
    bool    pipe_err;
    string  input_process;
    string  output_process;
    char*   pipe_argv[MAX_ARGS];
    int     pipe_argc;
    
    JobsList jobs;

    Shell(Shell const&)           = delete; // disable copy constructor
    void operator=(Shell const&)  = delete; // disable "=" operator
    static Shell& getInstance()             // make Shell singleton
    {
        static Shell instance;
        return instance;
    }
    ~Shell();

    void printPrompt() { cout << prompt << "> "; }
    void deleteArgs();

    void builtInCommand();
    void changeDirectory();
    void foregroundCommand();
    void quitCommand();
    void killCommand();
    void chmodCommand();
    void externalCommand();

    bool isRedirectionCommand(string& cmd_line);
    void redirectionCommand(string& cmd_line);
    
    bool isPipeCommand(string& cmd_line);
    void pipeCommand();

    void executeCommand(string cmd_line);
};

#endif //SMASH_COMMAND_H_
