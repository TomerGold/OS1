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

//function to handle foregrounded cmd which was interrupted by a signal
void handleInterruptedCmd(pid_t pid, Command* cmd, JobsList* jobsList) {
    if (sigINTOn) { // if FG process received ctrl+c
        kill(pid, SIGKILL);
        cout << "smash: process " << pid << " was killed" << endl;
        delete cmd;
        sigINTOn = false;
    } else if (sigSTPOn) { // if FG process received ctrl+z
        kill(pid, SIGSTOP);
        cout << "smash: process " << pid << " was stopped" << endl;
        jobsList->addJob(cmd, true);
        sigSTPOn = false;
    }
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

string getFirstArg(const char* cmd_line) {
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    string s;
    iss >> s;
    return s;
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

///Command functions:

Command::Command(const char* cmd_line) : isBackground(false),
                                         origCmd(cmd_line), pid(0),
                                         redirected(false), piped(false),
                                         stdOutCopy
                                                 (1) {
    for (int i = 0; i < COMMAND_MAX_ARGS + 1; ++i) {
        args[i] = NULL;
    }
    isBackground = _isBackgroundComamnd(cmd_line);
    IO_CHARS receivedType = containsSpecialChars();
    type = receivedType;
    if (receivedType == REDIR || receivedType == REDIR_APPEND) {
        redirected = true;
    }
    if (receivedType == PIPE || receivedType == PIPE_ERR) {
        piped = true;
    }
    char without_amper[COMMAND_ARGS_MAX_LENGTH + 1];
    strcpy(without_amper, cmd_line);
    _removeBackgroundSign(without_amper);
    argsNum = _parseCommandLine(without_amper, args);
}

IO_CHARS Command::containsSpecialChars() const {
    for (int i = 1; i < argsNum; i++) {
        if (strcmp(args[i], ">") == 0) {
            return REDIR;
        } else if (strcmp(args[i], ">>") == 0) {
            return REDIR_APPEND;
        } else if (strcmp(args[i], "|") == 0) {
            return PIPE;
        } else if (strcmp(args[i], "|&") == 0) {
            return PIPE_ERR;
        }
    }
    return NONE;
}

void Command::setOutputFD(const char* path, IO_CHARS type) {
    //TODO: should we handle empty/invalid path?
    if (type != REDIR && type != REDIR_APPEND) {
        return;
    }
    int stdoutCopy = dup(1);
    close(1);
    if (type == REDIR) {
        fopen(path, "w");
    } else { //must be REDIR_APPEND
        fopen(path, "a");
    }
    stdOutCopy = stdoutCopy;
}

void Command::restoreStdOut() {
    close(1);
    dup2(stdOutCopy, 1);
    close(stdOutCopy);
}

void ChangePrompt::execute() {
    if (args[1] == NULL) {
        smallShell->setPrompt(DEFAULT_PROMPT);
    }
    smallShell->setPrompt(args[1]);
}

void ShowPidCommand::execute() {
    cout << "smash pid is " << getpid() << endl;
}

void GetCurrDirCommand::execute() {
    char* currPath = get_current_dir_name();
    if (currPath == NULL) {
        perror("smash error: get_current_dir_name failed");
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
    if (args[1] == NULL) return; //got only cd without path
    if (strcmp(args[1], "-") == 0 && *lastPwd == NULL) {
        cout << "smash error: cd: OLDPWD not set" << endl;
        return;
    }
    char* currPath = get_current_dir_name();
    if (currPath == NULL) {
        perror("smash error: get_current_dir_name failed"); //TODO : make sure that
        // this error won't be a problem
        return;
    }
    if (strcmp(args[1], "-") == 0) { // to go back to last pwd
        if (chdir(*lastPwd) == -1) {
            perror("smash error: chdir failed");
            free(currPath);
            return;
        }
    } else { //if chdir arg is not "-"
        if (chdir(args[1]) == -1) {
            perror("smash error: chdir failed");
            free(currPath);
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
        return;
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
    Command* resumedCmd = toFG->getCommand();
    jobsList->removeJobById(jobId);
    if (kill(toFGPid, SIGCONT) == -1) { //TODO: if doesn't work replace
        // SIGCONT with 18
        perror("smash error: kill failed");
        return;
    }
    if (waitpid(toFGPid, NULL) == -1) { //was interrupted by signal
        handleInterruptedCmd(toFGPid, resumedCmd, jobsList);
    } else if (waitpid(pid, NULL, WNOHANG) ==
               pid) { //process finished successfully in foreground
        delete resumedCmd; //TODO: should work but it's weakpoint
    }
}

void BackgroundCommand::execute() {
    int jobId = 0;
    pid_t toBGPid = 0;
    JobsList::JobEntry* toBG = NULL;
    if (argsNum > 2) {
        cout << "smash error: bg: invalid arguments" << endl;
        return;
    }
    if (argsNum == 2) { // if jobId was specified
        try {
            jobId = stoi(args[1]);
        }
        catch (const std::exception& e) { // if jobId is not a number
            cout << "smash error: bg: invalid arguments" << endl;
            return;
        }
        if (jobId < 1) { // jobId is invalid
            cout << "smash error: bg: invalid arguments" << endl;
            return;
        }
        toBG = jobsList->getJobById(jobId);
        if (toBG == NULL) { // if requested jobId doesn't exist
            cout << "smash error: bg: job-id " << jobId
                 << " does not exist"
                 << endl;
            return;
        }
        if (toBG->getStatus() != STOPPED) {
            cout << "smash error: bg: job-id " << jobId << " is already "
                                                           "running in "
                                                           "the background"
                 << endl;
            return;
        }
    } else if (argsNum == 1 && !(jobsList->stoppedJobExists())) {
        cout << "smash error: bg: there is no stopped jobs to resume" <<
             endl;
        return;
    } else {
        toBG = jobsList->getLastStoppedJob(&jobId);
    }
    toBGPid = toBG->getPid();
    cout << toBG->getCommand()->getOrigCmd() << " : " << toBGPid << endl;
    kill(toBGPid, SIGCONT);
    toBG->setStatus(RUNNING);
}

void QuitCommand::execute() {
    if (argsNum > 1) {
        if (strcmp(args[1], "kill") == 0) {//kill was specified
            jobsList->killAllJobs();
        }
    }
    jobsList->destroyCmds();
    smash->setToQuit(true);
}

void ExternalCommand::execute() {
    string externCmd = "";
    int i = 0;
    while (args[i] != NULL) { // TODO: check that makes a correct command
        // for bash
        externCmd += args[i];
        externCmd += " ";
        i++;
    }
    const char* bashArgs[3] = {"bash", "-c", externCmd.c_str()};
    pid_t pid = fork();
    if (pid == -1) {
        perror("smash error: fork failed");
        return;
    }
    if (pid == 0) { // child process
        if (isRedirected()) {
            setOutputFD(getPath(), type); //has to be ">" // or ">>"
        }
        setpgrp();
        execv("/bin/bash", bashArgs);
        perror("smash error: execv failed");
    } else { // smash process
        this->pid = pid;
        if (isBackgroundCmd()) {//should not wait and add to jobsList
            jobsList->addJob(this);
        } else {//should run in the foreground and wait for child to finish
            if (waitpid(pid, NULL) == -1) {//was interrupted by a signal
                handleInterruptedCmd(pid, this, jobsList);
            } else if (waitpid(pid, NULL, WNOHANG) ==
                       pid) { //process finished successfully in foreground
                delete this; //TODO: should work but it's weakpoint
            }
        }
    }
}


///Jobs list functions:

JobsList::JobsList() : maxId(0), jobsList(0) {
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
        *lastJobId = 0;
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

bool JobsList::stoppedJobExists() const {
    for (auto iter = jobsList.begin();
         iter != jobsList.end(); ++iter) {
        if (iter->getStatus() == STOPPED) {
            return true;
        }
    }
    return false;
}

JobsList::JobEntry* JobsList::getLastStoppedJob(int* jobId) {
    JobsList::JobEntry* maxStoppedJob = NULL;
    for (auto iter = jobsList.begin(); iter != jobsList.end();
         ++iter) {
        if (iter->getStatus() == STOPPED) {
            maxStoppedJob = &(*iter);
        }
    }
    if (maxStoppedJob != NULL) {
        *jobId = maxStoppedJob->getJobId();
    }
    return maxStoppedJob;
}

void JobsList::removeFinishedJobs() {
    //TODO: ask in the piazza what happens if proccess finished after
    // passing it in the loop
    if (jobsList.empty()) return;
    auto iter = jobsList.begin();
    while (iter != jobsList.end()) {
        if (waitpid(iter->getPid(), NULL, WNOHANG) == iter->getPid()) {
            auto toDelete = iter++;
            delete toDelete->getCommand();
            jobsList.remove(*toDelete);
            continue;
        }
        ++iter;
    }
    getLastJob(&maxId); //updates maxId to the last job jobId
}

void JobsList::killAllJobs() {
    //TODO: should print something if jobsList is empty???
    cout << "smash: sending SIGKILL signal to " << jobsList.size()
         << "jobs:" << endl;
    for (auto iter = jobsList.begin(); iter != jobsList.end();
         ++iter) {
        pid_t currPid = iter->getPid();
        cout << currPid << ": " <<
             iter->getCommand()->getOrigCmd() << endl;
        if (kill(currPid, SIGKILL) == -1) {
            perror("smash error: kill failed");
            return;
        }
    }
}

void JobsList::destroyCmds() {
    for (auto iter = jobsList.begin(); iter != jobsList.end();
         ++iter) {
        delete iter->getCommand(); // should delete all cmds generated
        // and entered to the jobsList (shouldn't be other cmds);
    }
}

void JobsList::addJob(Command* cmd, bool isStopped) {
    removeFinishedJobs();
    STATUS status = isStopped ? STOPPED : RUNNING;
    JobEntry toAdd(++maxId, cmd->getPid(), cmd, status);
    jobsList.push_back(toAdd);
}

///Smash functions:

SmallShell::SmallShell() : prompt(DEFAULT_PROMPT), lastPwd(NULL),
                           jobsList(), toQuit(false) {
}

SmallShell::~SmallShell() {
    free(lastPwd);
// TODO: add your implementation
}

Command* SmallShell::CreateCommand(const char* cmd_line) {
    string cmd_s = string(cmd_line);
    string cmd_trimmed = _trim(cmd_s);
    string cmdOnly = getFirstArg(cmd_line);
    //find return The position of the first character of the first match
    if (cmd_trimmed.find('|') != string::npos) {
        return new PipeCommand(cmd_line, &jobsList);
    }
    if (cmdOnly == "pwd") {
        return new GetCurrDirCommand(cmd_line);
    }
    if (cmdOnly == "chprompt") {
        return new ChangePrompt(cmd_line, this);
    }
    if (cmdOnly == "showpid") {
        return new ShowPidCommand(cmd_line);
    }
    if (cmdOnly == "cd") {
        return new ChangeDirCommand(cmd_line, &lastPwd);
    }
    if (cmdOnly == "jobs") {
        return new JobsCommand(cmd_line, &jobsList);
    }
    if (cmdOnly == "kill") {
        return new KillCommand(cmd_line, &jobsList);
    }
    if (cmdOnly == "fg") {
        return new ForegroundCommand(cmd_line, &jobsList);
    }
    if (cmdOnly == "bg") {
        return new BackgroundCommand(cmd_line, &jobsList);
    }
    if (cmdOnly == "quit") {
        return new QuitCommand(cmd_line, &jobsList, this);
    }
        //TODO: enter cp here
    else { // External Cmds
        return new ExternalCommand(cmd_line, &jobsList);
    }
}

void SmallShell::executeCommand(const char* cmd_line) {
    if (_trim(cmd_line).empty()) { //no cmd received, go get next cmd
        return;
    }     //TODO: ask if that what we should do in the piazza!
    Command* cmd = CreateCommand(cmd_line);
    // TODO: check if foreground/background
    jobsList.removeFinishedJobs();
    IO_CHARS cmdIOType = cmd->getType();
    if (cmd->isPiped()) {
        string stringCmd = (string) (cmd_line);
        int pipeIndex = stringCmd.find('|');
        string firstCmd = stringCmd.substr(0, pipeIndex);
        string secondCmd;
        if (cmdIOType == PIPE) { //TODO: verify staring location of
            // second cmd , is it really pipeIndex + 1?
            secondCmd = stringCmd.substr(pipeIndex + 1);
        } else {//must be PIPE_ERR //TODO: verify also here (|&)
            secondCmd = stringCmd.substr(pipeIndex + 2);
        }
        ((PipeCommand*) (cmd))->setFirstCmd( //converting to PipeCommand
                // to access setCmds member functions
                CreateCommand(firstCmd.c_str()));
        ((PipeCommand*) (cmd))->setSecondCmd(
                CreateCommand(secondCmd.c_str()));
        //TODO: exec each one
        //TODO: check if there '&' in the second command and if
        // there run pipe in background as new job and both subcommands
        // should also run in the background without being in the jobsList
    }
    if (dynamic_cast<BuiltInCommand*>(cmd) != NULL) {
        if (cmd->isRedirected()) { // redirect stdout to the
            // requested file
            cmd->setOutputFD(cmd->getPath(), cmdIOType);
        }
    }
    cmd->execute();
    if (dynamic_cast<BuiltInCommand*>(cmd) != NULL) {//TODO: need to
        // think if there is another way to delete finished cmds which
        // are not in the jobsList
        if (cmd->isRedirected()) { //restore stdout to channel 1 in FDT
            cmd->restoreStdOut();
        }
        delete cmd;
    }

    // TODO: remember to delete external Command if foreground (not
    //  joblisted)
    // Please note that you must fork smash process for some commands (e.g., external commands....)

}
