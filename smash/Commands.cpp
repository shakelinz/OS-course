#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <sys/stat.h>
#include <iomanip>
#include <fcntl.h>
#include "Commands.h"

using namespace std;

#define SAFE_SYSCALL_INT(syscall, name) do          \
{                                                   \
    if ((syscall) == -1)                            \
    {                                               \
        perror("smash error: " #name " failed");    \
        return;                                     \
    }                                               \
} while (0);                                        \


#define SAFE_SYSCALL_PTR(syscall, name) do          \
{                                                   \
    if ((syscall) == nullptr)                       \
    {                                               \
        perror("smash error: " #name " failed");    \
        return;                                     \
    }                                               \
} while (0);                                        \


void printArgs(int argc, char** argv)
{
    for (int i = 0; i < argc; i++)
        cout << "arg["<<i<<"] = |" << argv[i] << "|\n";
}

// string stuff:
const string WHITESPACE = " \n\r\t\f\v";

string ltrim(const string& s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == string::npos) ? "" : s.substr(start);
}

string rtrim(const string& s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == string::npos) ? "" : s.substr(0, end + 1);
}

string trim(const string& s)
{
  return rtrim(ltrim(s));
}

int parseLine(const string cmd_line, char** args)
{
    int i = 0;
    istringstream iss(trim(cmd_line).c_str());
    for(string s; iss >> s; )
    {
        args[i] = (char*)malloc(s.length()+1);
        memset(args[i], 0, s.length()+1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;
}

bool isBackgroundCommand(const char* cmd_line)
{
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void removeBackgroundSign(string& cmd_line)
{
    const string str(cmd_line);
    unsigned int idx = str.find_last_not_of(WHITESPACE);      // find last non-whitespace character
    if (idx == string::npos) return;                          // return if all characters are spaces
    if (cmd_line[idx] != '&') return;                         // return if command line doesn't end with '&'
    cmd_line[idx] = ' ';                                      // replace '&' with space
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;  // truncate the command line string up to the last non-space character
}

bool isNumber(const string& str)
{
    if (str[0] == '-')
        return str.substr(1).find_first_not_of("0123456789") == string::npos;
    return str.find_first_not_of("0123456789") == string::npos;
}

bool removeAmpersand(string& cmd_line)
{
    const string str(cmd_line);
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    if (cmd_line[idx] != '&') return false;
    cmd_line[idx] = ' ';                                      // replace '&' with space
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;  // truncate the command line string up to the last non-space character
    return true;
}


JobsList::Job::Job(pid_t pid, string cmd)
    : pid(pid), cmd(cmd) {}

JobsList::Job* JobsList::getJobById(int id)
{
    for (auto& element : list)
    {
        if (element->id == id)
            return element;
    }
    return nullptr;
}

void JobsList::removeFinishedJobs()
{
    pid_t pid, waitpid_result;
    for (unsigned int i = 0; i < list.size(); i++)
    {
        pid = list[i]->pid;
        waitpid_result = waitpid(pid, nullptr, WNOHANG);
        SAFE_SYSCALL_INT(waitpid_result, waitpid);
        if (waitpid_result > 0)
        {
            list.erase(list.begin() + i);
            i--;
        }
    }
}

void JobsList::printJobsList()
{
    // removeFinishedJobs();
    for (auto job : list)
        cout << "[" << job->id << "] " << job->cmd << endl;
}

void JobsList::add(pid_t pid, string cmd)
{
    // removeFinishedJobs();
    Job* job = new Job(pid, cmd);
    job->id = (list.empty()) ? 1 : list.back()->id + 1;
    job->running = true;
    list.push_back(job);
}

void JobsList::remove(int id)
{
    for (unsigned int i = 0; i < list.size(); i++)
        if (list[i]->id == id)
            list.erase(list.begin() + i);
}


bool isBuiltInCommand(string cmd)
{
    return
    (
        cmd.compare( "chprompt" ) == 0 ||
        cmd.compare( "showpid"  ) == 0 ||
        cmd.compare( "pwd"      ) == 0 ||
        cmd.compare( "cd"       ) == 0 ||
        cmd.compare( "jobs"     ) == 0 ||
        cmd.compare( "fg"       ) == 0 ||
        cmd.compare( "quit"     ) == 0 ||
        cmd.compare( "kill"     ) == 0 ||
        cmd.compare( "chmod"    ) == 0
    );
}

bool isComplexCommand(string cmd_line)
{
    unsigned int idx = cmd_line.find_first_of("*?");
    return (idx <= cmd_line.length());
}


Shell::Shell()
    : prompt("smash"),
      pipe_err(false)
{
    memset(previous, 0, MAX_PATH);
    pid = getpid();
    SAFE_SYSCALL_INT(pid, getpid);
}

Shell::~Shell() {}

void Shell::deleteArgs()
{
    for (int i = 0; i < argc; i++)
        delete argv[i];
}

bool Shell::isRedirectionCommand(string& cmd_line)
{
    append = false;
    unsigned int idx = cmd_line.find_first_of(">");
    if (idx > cmd_line.length()) return false;
    cmd_line[idx] = ' ';
    if (cmd_line[idx + 1] == '>')
    {
        append = true;
        cmd_line[idx + 1] = ' ';
    }
    output_file = trim(cmd_line.substr(idx));
    cmd_line = trim(cmd_line.substr(0, idx));
    return true;
}

void Shell::redirectionCommand(string& cmd_line)
{
    stdout_index = dup(STDOUT_FILENO);
    SAFE_SYSCALL_INT(stdout_index, dup)
    
    SAFE_SYSCALL_INT(close(STDOUT_FILENO), close)

    int flags = O_WRONLY | O_CREAT;
    if (append) flags |= O_APPEND;
    else flags |= O_TRUNC;

    SAFE_SYSCALL_INT(open(output_file.c_str(), flags, 0755), open)

    executeCommand(cmd_line);

    SAFE_SYSCALL_INT(close(STDOUT_FILENO), close)
    SAFE_SYSCALL_INT(dup2(stdout_index, STDOUT_FILENO), dup2)
    SAFE_SYSCALL_INT(close(stdout_index), close)
}

bool Shell::isPipeCommand(string& cmd_line)
{
    pipe_err = false;
    unsigned int idx = cmd_line.find_first_of("|");
    if (idx > cmd_line.length()) return false;
    cmd_line[idx] = ' ';
    if (cmd_line[idx + 1] == '&')
    {
        pipe_err = true;
        cmd_line[idx + 1] = ' ';
    }
    input_process = trim(cmd_line.substr(idx));
    output_process = trim(cmd_line.substr(0, idx));
    return true;
}

void Shell::pipeCommand()
{
    int my_pipe[2];
    int write_destination = pipe_err ? STDERR_FILENO : STDOUT_FILENO;

    SAFE_SYSCALL_INT(pipe(my_pipe), pipe);

    int output_backup = dup(write_destination);
    SAFE_SYSCALL_INT(output_backup, dup);

    SAFE_SYSCALL_INT(dup2(my_pipe[1], write_destination), dup2);

    executeCommand(output_process);

    SAFE_SYSCALL_INT(dup2(output_backup, write_destination), dup2);

    SAFE_SYSCALL_INT(close(my_pipe[1]), close);

    int input_backup = dup(STDIN_FILENO);
    SAFE_SYSCALL_INT(input_backup, dup);
    
    SAFE_SYSCALL_INT(dup2(my_pipe[0], STDIN_FILENO), dup2);

    executeCommand(input_process);
    
    SAFE_SYSCALL_INT(close(my_pipe[0]), close);

    SAFE_SYSCALL_INT(dup2(input_backup, STDIN_FILENO), dup2);
}

void Shell::foregroundCommand()
{
    int size = jobs.list.size();
    JobsList::Job* job;
    
    int i = 0;
    if (argv[1] && isNumber(argv[1]))
    {
        int id = stoi(argv[1]);

        for (; i < size; i++)
            if (jobs.list[i]->id == id)
                break;
        
        if (i >= size)
        {
            cerr << "smash error: fg: job-id "<< id << " does not exist\n";
            return;
        }

        job = jobs.list[i];
    }
    else if (argc == 1)
    {
        if (size == 0)
        {
            cerr << "smash error: fg: jobs list is empty\n";
            return;
        }
        else
        {
            job = jobs.list.back();
            i = jobs.list.size() - 1;
        }
    }

    if
    (
        argc > 2 ||
        (argc == 2 && !isNumber(argv[1]))
    )
    {
        cerr << "smash error: fg: invalid arguments\n";
        return;
    }

    fg_pid = job->pid;
    cout << job->cmd << " " << job->pid << "\n";
    SAFE_SYSCALL_INT(waitpid(job->pid, nullptr, 0), waitpid);
    jobs.list.erase(jobs.list.begin() + i);
    fg_pid = 0;
}

void Shell::changeDirectory()
{
    if (argc > 2)
    {
        cerr << "smash error: cd: too many arguments\n";
        return;
    }
    else if (argc < 2)
        return;
    else
    {
        string new_path = string(argv[1]);
        char*  old_path = getcwd(nullptr, 0);
        SAFE_SYSCALL_PTR(old_path, getcwd);
        int syscall_result;
        
        if (new_path.compare("-") == 0)
        {
            if (previous[0] == 0)
            {
                cerr << "smash error: cd: OLDPWD not set\n";
                return;
            }
            else
                syscall_result = chdir(previous);
        }
        else
            syscall_result = chdir(new_path.c_str());
        
        SAFE_SYSCALL_INT(syscall_result, chdir);
        strcpy(previous, old_path);
    }
}

void Shell::quitCommand() 
{
    if
    (
        argv[1] != nullptr &&
        string(argv[1]).compare("kill") == 0
    )
    {
        pid_t pid;
        string cmd;
        int jobs_count = jobs.list.size();
        
        cout << "smash: sending SIGKILL signal to " << jobs_count << " jobs:\n";
        
        while(jobs_count != 0)
        {
            pid = jobs.list.front()->pid;
            cmd = jobs.list.front()->cmd;
            cout << pid << ": " << cmd << endl;
            SAFE_SYSCALL_INT(kill(pid, 9), kill);
            SAFE_SYSCALL_INT(waitpid(pid, nullptr, 0), waitpid);
            jobs.list.erase(jobs.list.begin());
            jobs_count = jobs.list.size();
        }
    }
    exit(0);
}

void Shell::killCommand()
{
    int i = 0;
    if (argv[2])
    {
        if (!isNumber(argv[2]))
        {
            cerr << "smash error: kill: invalid arguments\n";
            return;
        }

        int id = stoi(argv[2]);
        int size = jobs.list.size();
        for (; i < size; i++)
                if (jobs.list[i]->id == id)
                    break;
        if (i >= size)
        {
            cerr << "smash error: kill: job-id "<< id << " does not exist\n";
            return;
        }
    }

    if
    (
        argc != 3 ||
        argv[1][0] != '-' ||
        !isNumber(string(argv[1]).substr(1))
    )
    {
        cerr << "smash error: kill: invalid arguments\n";
        return;
    }

    int signal = stoi(string(argv[1]).substr(1));

    SAFE_SYSCALL_INT(kill(jobs.list[i]->pid, signal), kill);
    cout << "signal number " << signal << " was sent to pid " << jobs.list[i]->pid << endl;
    jobs.list.erase(jobs.list.begin() + i);
}

void Shell::chmodCommand()
{
    if
    (
        argc != 3 ||
        !isNumber(argv[1]) ||
        stoi(argv[1]) < 0 ||
        stoi(argv[1]) > 7777
    )
    {
        cerr << "smash error: chmod: invalid arguments\n";
        return;
    }
    SAFE_SYSCALL_INT(chmod(argv[2], (mode_t)stoi(argv[1], 0, 8)), chmod);
}

void Shell::externalCommand()
{
    pid_t pid = fork();
    SAFE_SYSCALL_INT(pid, fork);
    
    // rebuild command line:
    string cmd_copy = string(argv[0]);
    for (int i = 1; i < argc; i++)
    {
        cmd_copy.append(" ");
        cmd_copy.append(string(argv[i]));
    }
    
    if (pid == 0) // child process
    {
        if (setpgrp() == -1)
        {
            perror("smash error: setpgrp failed");
            exit(0);
        }
        
        int execvp_result;
        if (complex)
        {
            char bash[] = "bash";
            char dash_c[] = "-c";
            char* args[4] = {bash, dash_c, const_cast<char*>(cmd_copy.c_str()), nullptr};
            execvp_result = execvp(bash, args);
        }
        else
            execvp_result = execvp(argv[0], argv);
        
        if (execvp_result == -1)
            perror("smash error: execvp failed");
        
        exit(0);
    }
    else // parent process
    {
        if (background)
        {
            jobs.add(pid, command);
        }
        else
        {
            fg_pid = pid;
            pid_t waitpid_result = waitpid(pid, nullptr, 0);
            SAFE_SYSCALL_INT(waitpid_result, waitpid);
            fg_pid = 0;
        }
    }
}

void Shell::builtInCommand()
{
    string cmd = string(argv[0]);

    if (cmd.compare("showpid") == 0)
        cout << "smash pid is " << pid << endl;
    
    else if (cmd.compare("chprompt") == 0)
        prompt = (argv[1]) ? argv[1] : "smash";
    
    else if (cmd.compare("pwd") == 0)
    {
        char* path = getcwd(nullptr, 0);
        SAFE_SYSCALL_PTR(path, getcwd);
        cout << path << endl;
    }
    
    else if (cmd.compare("cd") == 0)
        changeDirectory();

    else if (cmd.compare("jobs") == 0)
        jobs.printJobsList();

    else if (cmd.compare("fg") == 0)
        foregroundCommand();
    
    else if (cmd.compare("quit") == 0)
        quitCommand();

    else if (cmd.compare("kill") == 0)
        killCommand();
    
    else if (cmd.compare("chmod") == 0)
        chmodCommand();
}

void Shell::executeCommand(string cmd_line)
{
    command = cmd_line;
    if (trim(cmd_line).empty()) return;         // return if cmd_line is empty
    background = removeAmpersand(cmd_line);     // remove '&' and return true
    complex = isComplexCommand(cmd_line);
    
    argc = parseLine(cmd_line, argv);           // parse command line and get number of arguments
    jobs.removeFinishedJobs();                  // lol, always do this!
    
    if (isBuiltInCommand(string(argv[0])))      // handle built-in command and return
        { builtInCommand(); return; }

    string cmd = string(argv[0]);               /////////////////////////
                                                //
    if (cmd.compare("q") == 0)                  //
    {                                           //  remove all this shit
        cout << "Bye!\n";                       //
        exit(0);                                //
    }                                           //
    // else if (cmd.compare("test") == 0)       //
    //     { /* test some shit */ }             /////////////////////////

    else externalCommand();                     // "else" unnecessary
}