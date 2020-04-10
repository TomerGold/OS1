#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"

using namespace std;

#define DEFAULT_PROMPT "smash"

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cerr << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cerr << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

#define DEBUG_PRINT cerr << "DEBUG: "

#define EXEC(path, arg) \
  execvp((path), (arg));

string _ltrim(const std::string& s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s) {
    return _rtrim(_ltrim(s));
}

//converting the cmd string to cmd and options in the array args,
// args[0] is the cmd.
int _parseCommandLine(const char* cmd_line, char** args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char*) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char* cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

// TODO: Add your implementation for classes in Commands.h

Command::Command(const char* cmd_line) : isBackground(false),
                                         origCmd(cmd_line) {
    isBackground = _isBackgroundComamnd(cmd_line);
    char* without_amper;
    strcpy(without_amper, cmd_line);
    _removeBackgroundSign(without_amper);
    argsNum = _parseCommandLine(without_amper, args);
}

void ChangePrompt::execute() {
    if (args[1] == NULL) {
        s->setPrompt(DEFAULT_PROMPT);
    }
    s->setPrompt(args[1]);
}

void ShowPidCommand::execute() {
    cout << "smash pid is " << getpid() << endl;
}

void GetCurrDirCommand::execute() {
    char* currPath = getcwd(NULL, 0); //TODO: if pwd doesn't work,
    // check getcwd inputs
    if (currPath == NULL) {
        perror("smash error: getcwd failed");
        return;
    }
    cout << currPath << endl;
    free(currPath);
}

void ChangeDirCommand::execute() { // TODO: check what we should do if
    // there is a & in the end of the given pwd
    if (argsNum > 2) {
        cout << "smash error: cd: too many arguments" << endl;
    }
    if (strcmp(args[1], "-") == 0 && *lastPwd == NULL) {
        cout << "smash error: cd: OLDPWD not set" << endl;
        return;
    }
    char* currPath = getcwd(NULL, 0);
    if (currPath == NULL) {
        perror("smash error: getcwd failed"); //TODO : make sure that
        // this error won't be a problem
        return;
    }
    if (strcmp(args[1], "-") == 0) { // to go back to last pwd
        if (chdir(*lastPwd) == -1) {
            perror("smash error: chdir failed");
            return;
        }
    } else { //if chdir arg is not "-"
        //TODO : check if there is need to handle ".."
        if (chdir(args[1]) == -1) {
            perror("smash error: chdir failed");
            return;
        }
    }
    if (*lastPwd != NULL) { // for cd in the first time to not free NULL
        free(*lastPwd);
    }
    *lastPwd = currPath;
}

void JobsCommand::execute() {
    jobsList->removeFinishedJobs();
    jobsList->printJobsList();
}

void KillCommand::execute() {
    //TODO: check if after kill -9 job is removed from jobLists
    int sigNum = 0;
    int jobId = 0;
    try {
        sigNum = stoi(args[1]) * (-1);
        jobId = stoi(args[2]);
    }
    catch (const std::exception& e) {
        cout << "smash error: kill: invalid arguments" << endl;
        return;
    }
    if (argsNum > 3 || sigNum < 0 || sigNum > 31) {
        cout << "smash error: kill: invalid arguments" << endl;
        return;
    }
    JobsList::JobEntry* toKill = jobsList->getJobById(jobId);
    if (toKill == NULL) {
        cout << "smash error: kill: job-id " << jobId << " does not "
                                                         "exist" << endl;
    }
    if (kill(toKill->getPid(), sigNum) == -1) {
        perror("smash error: kill failed");
        return;
    }
    cout << "signal number " << sigNum << " was sent to pid "
         << toKill->getPid() << endl;
}

void ForegroundCommand::execute() {
    int jobId = 0;
    pid_t toFGPid = 0;
    JobsList::JobEntry* toFG = NULL;
    if (argsNum > 2) {
        cout << "smash error: fg: invalid arguments" << endl;
        return;
    }
    if (argsNum == 2) { // if jobId was specified
        try {
            jobId = stoi(args[1]);
        }
        catch (const std::exception& e) { // if jobId is not a number
            cout << "smash error: fg: invalid arguments" << endl;
            return;
        }
        if (jobId < 1) { // jobId is invalid
            cout << "smash error: fg: invalid arguments" << endl;
            return;
        }
        toFG = jobsList->getJobById(jobId);
        if (toFG == NULL) { // if requested jobId doesn't exist
            cout << "smash error: fg: job-id " << jobId
                 << " does not exist"
                 << endl;
            return;
        }
    } else if (argsNum == 1 && jobsList->isJobListEmpty()) {
        cout << "smash error: fg: jobs list is empty" << endl;
        return;
    } else {
        toFG = jobsList->getLastJob(&jobId);
    }
    toFGPid = toFG->getPid();
    cout << toFG->getCommand()->getOrigCmd() << " : " << toFGPid << endl;
    jobsList->removeJobById(jobId);
    if (kill(toFGPid, SIGCONT) == -1) { //TODO: if doesn't work replace
        // SIGCONT with 18
        perror("smash error: kill failed");
        return;
    }
    if (waitpid(toFGPid, NULL) == -1) {
        perror("smash error: waitpid failed");
        return;
    }
}

JobsList::JobsList() : maxId(0), jobsList() {
}

JobsList::JobEntry* JobsList::getJobById(int jobId) {
    for (auto iter = jobsList.begin(); iter != jobsList.end();
         ++iter) {
        if (iter->getJobId() == jobId) {
            return &(*iter); //TODO: weakpoint, we're not sure about the &*
        }
    }
    return NULL;
}

JobsList::JobEntry* JobsList::getLastJob(int* lastJobId) {
    if (jobsList.empty()) {
        *lastJobId = -1;
        return NULL;
    }
    *lastJobId = jobsList.back().getJobId();
    return &(jobsList.back()); //TODO: may not work because back return
    // reference to the last object and we want to return the address to it
}

void JobsList::printJobsList() {
    for (auto iter = jobsList.begin(); iter != jobsList.end();
         ++iter) {
        time_t currTime = time(NULL);
        if (currTime == (time_t) (-1)) {
            perror("smash error: time failed");
            return;
        }
        int elapsedTime = difftime(iter->getStartTime(), currTime);
        //TODO : check if it is ok to use int
        cout << "[" << iter->getJobId() << "] " <<
             iter->getCommand()->getOrigCmd() << " : " <<
             iter->getPid() << " " << elapsedTime << " secs";
        if (iter->getStatus() == STOPPED) {
            cout << " (stopped)";
        }
        cout << endl;
    }
}

void JobsList::removeJobById(int jobId) {
    for (auto iter = jobsList.begin(); iter != jobsList.end();
         ++iter) {
        if (iter->getJobId() == jobId) {
            jobsList.remove(*iter);
            if (jobsList.empty()) {
                maxId = 0;
            }
            maxId = jobsList.back().getJobId();
            return;
        }
    }
}

SmallShell::SmallShell() : prompt(DEFAULT_PROMPT), lastPwd(NULL),
                           jobsList() {
// TODO: add your implementation
}

SmallShell::~SmallShell() {
    free(lastPwd);
// TODO: add your implementation
}


/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command* SmallShell::CreateCommand(const char* cmd_line) {
    // For example:

    string cmd_s = string(cmd_line);
    string cmd_trimmed = _trim(cmd_s);
    //TODO: check if args[0] == "" and handle this
    //find return The position of the first character of the first match
    if (cmd_trimmed.find("pwd") == 0) {
        return new GetCurrDirCommand(cmd_line);
    }
    if (cmd_trimmed.find("chprompt") == 0) {
        return new ChangePrompt(cmd_line, this);
    }
    if (cmd_trimmed.find("showpid") == 0) {
        return new ShowPidCommand(cmd_line);
    }
    if (cmd_trimmed.find("cd") == 0) {
        return new ChangeDirCommand(cmd_line, &lastPwd);
    }
    if (cmd_trimmed.find("jobs") == 0) {
        return new JobsCommand(cmd_line, &jobsList);
    }
    if (cmd_trimmed.find("kill") == 0) {
        return new KillCommand(cmd_line, &jobsList);
    }
    if (cmd_trimmed.find("fg") == 0) {
        return new ForegroundCommand(cmd_line, &jobsList);
    }


//  else if ...
//  .....
//  else {
//    return new ExternalCommand(cmd_line);
//  }
//    return nullptr;
}

void SmallShell::executeCommand(const char* cmd_line) {
    // TODO: Add your implementation here
    // for example:
    Command* cmd = CreateCommand(cmd_line);
    // TODO: check if foreground/background
    cmd->execute();
    // TODO: delete Command if foreground (not joblisted)
    // Please note that you must fork smash process for some commands (e.g., external commands....)

}
