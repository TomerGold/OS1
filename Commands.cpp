#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <fcntl.h>
#include <iomanip>
#include "Commands.h"
#include "signals.h"

using namespace std;

pid_t foregroundPid = 0;
bool isForegroundPipe = false;
pid_t pipeFirstCmdPid = NOT_FORKED;
pid_t pipeSecondCmdPid = NOT_FORKED;

string defPrompt = "smash";

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


string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

//function to handle foregrounded cmd which was interrupted by a signal
// if NULL is sent as currJob that mean it was an external cmd in
// foreground. in case of SIGSTP a new job will be added to job list in
// the end of the list, else just update the JobEntry status
void handleInterruptedCmd(pid_t pid, Command *cmd,
                          JobsList::JobEntry *currJob,
                          JobsList *jobsList) {
    if (sigINTOn) { // if FG process received ctrl+c
        if (currJob == NULL) {
            delete cmd;
        } else {
            delete cmd;
            jobsList->removeJobById(currJob->getJobId());
        }
        sigINTOn = false;

        /*if job entry exists, that mean the process was foregrounded
        from the background, and now it's a zombie, so it will be
        deleted in the next removeFinished call*/

    } else if (sigSTPOn) { // if FG process received ctrl+z
        if (currJob == NULL) { // wasn't foregrounded
            jobsList->addJob(cmd, pid, true);
        } else { // was foregrounded by fg command
            currJob->setStatus(STOPPED);
            currJob->setStartTimeNow();
        }
        sigSTPOn = false;
    }
    foregroundPid = 0;
    isForegroundPipe = false;
}

void handleInterruptedCmdPipe(Command *cmd) {

    if (sigINTOn) { // pipe should delete sons cmds
        sigINTOn = false;
        delete cmd;
        kill(getpid(), SIGKILL); //pipe commit suicide
    }

    //no need to handle SIGTSTP, it is handled in the signal handler in signals.cpp
}

//converting the cmd string to cmd and options in the array args,
// args[0] is the cmd.
int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

string getFirstArg(const char *cmd_line) {
    //TODO: should we handle connected redirection cmd like cmd>txt.1
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    string s;
    iss >> s;
    if (s.find('&') == (s.size() - 1)) {
        s.erase(s.size() - 1);
    }
    return s;
}

bool _isBackgroundComamnd(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
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

char *createExternCmd(char *const *args) {
    string externCmd;
    int i = 0;
    while (args[i] != NULL) {
        if (args[i][0] == '>') break;
        externCmd += args[i];
        if (args[i + 1] != NULL) {
            externCmd += " ";
        }
        i++;
    }
    char *externCmdStr = (char *) malloc(externCmd.size() + 1);
    strcpy(externCmdStr, externCmd.c_str());
    externCmdStr[externCmd.size()] = '\0';
    return externCmdStr;
}

char **createBashArgs(char *const *args) {
    char **bashArgs = (char **) malloc(4 * sizeof(char *));
    char *externCmdStr = createExternCmd(
            args); //have to do this because of execv demands
    char *bash = (char *) malloc(10 * sizeof(char));
    string bashStr = "/bin/bash";
    strcpy(bash, bashStr.c_str());
    bash[9] = '\0';

    char *cOption = (char *) malloc(3 * sizeof(char));
    string cOptionStr = "-c";
    strcpy(cOption, cOptionStr.c_str());
    cOption[2] = '\0';

    bashArgs[0] = bash;
    bashArgs[1] = cOption;
    bashArgs[2] = externCmdStr;
    bashArgs[3] = NULL;
    return bashArgs;
}

void freeBashArgs(char **bashArgs) {
    for (int i = 0; i < 3; i++) {
        free(bashArgs[i]);
    }
    free(bashArgs);
    *bashArgs = NULL;
}

void pipeManageFD(FDT_CHANNEL FDToCLose, int newFD, IO_CHARS pipeType) {
    bool failed = false;
    if (FDToCLose == IN) {
        if (close(0) == -1) { // should close stdin
            perror("smash error: close failed");
            failed = true;
        }
    } else if (pipeType == PIPE) { //should close stdout
        if (close(1) == -1) {
            perror("smash error: close failed");
            failed = true;
        }
    } else { // should close stderr
        if (close(2) == -1) {
            perror("smash error: close failed");
            failed = true;
        }
    }
    if (dup(newFD) == -1) {
        perror("smash error: dup failed");
        failed = true;
    }
    if (close(newFD) == -1) {
        perror("smash error: close failed");
        failed = true;
    }
    if (failed) exit(0); // TODO: make sure this is ok to just exit pipe in case of built in cmds
}

bool isArgumentExist(char **args, const string &toFind) {
    int i = 0;
    while (args[i] != NULL) {
        if (toFind == args[i]) {
            return true;
        }
        i++;
    }
    return false;
}

void cpMain(char *const *args) {
    if (strcmp(args[1], args[2]) == 0) {
        cout << "smash: " << args[1] << " was copied to " << args[2] <<
             endl;
        exit(0);
    }
    char buffer[1];
    int fds[2];
    size_t buf_size = 1;
    ssize_t status;
    fds[0] = open(args[1], O_RDONLY);
    if (fds[0] == -1) {
        perror("smash error: open failed");
        exit(0);
    }
    fds[1] = open(args[2], O_WRONLY | O_CREAT | O_TRUNC, 0777);
    //TODO: check mode thingy in last line
    if (fds[1] == -1) {
        perror("smash error: open failed");
        close(fds[0]);
        exit(0);
    }
    while ((status = read(fds[0], buffer, buf_size)) > 0) {
        if ((write(fds[1], buffer, buf_size)) == -1) {
            perror("smash error: write failed");
            close(fds[0]);
            close(fds[1]);
            exit(0);
        }
    }
    if (status == -1) {//read failed
        perror("smash error: read failed");
        close(fds[0]);
        close(fds[1]);
        exit(0);
    }
    close(fds[0]);
    close(fds[1]);
    cout << "smash: " << args[1] << " was copied to " << args[2] <<
         endl;
    exit(0);
}
///Command functions:

Command::Command(const char *cmd_line) : isBackground(false),
                                         origCmd(cmd_line), redirected
                                                 (false), piped(false),
                                         stdOutCopy(1) {
    for (int i = 0; i < COMMAND_MAX_ARGS + 1; ++i) {
        args[i] = NULL;
    }
    isBackground = _isBackgroundComamnd(cmd_line);
    char without_amper[COMMAND_ARGS_MAX_LENGTH + 1];
    strcpy(without_amper, cmd_line);
    _removeBackgroundSign(without_amper);
    argsNum = _parseCommandLine(without_amper, args);
    IO_CHARS receivedType = containsSpecialChars();
    type = receivedType;
    if (receivedType == REDIR || receivedType == REDIR_APPEND) {
        redirected = true;
    }
    if (receivedType == PIPE || receivedType == PIPE_ERR) {
        piped = true;
    }
}

IO_CHARS Command::containsSpecialChars() const {
    for (int i = 1; i < argsNum; i++) {
        if (args[i] == NULL) break;
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

bool Command::setOutputFD(const char *path, IO_CHARS type) {
    //TODO: do we need to check return value of fopen? if yes, and on
    // failure should do nothing remember to handle this in external
    // and cp execute, and in executeCommand (of built in redirection)
    if (type != REDIR && type != REDIR_APPEND) {
        return true;
    }
    stdOutCopy = dup(1);
    if (stdOutCopy == -1) {
        perror("smash error: dup failed");
        return false;
    }
    if (close(1) == -1) {
        perror("smash error: close failed");
        return false;
    }
    if (type == REDIR) {
        if (fopen(path, "w") == NULL) {
            perror("smash error: open failed");
            return false;
        }
    } else { //must be REDIR_APPEND
        if (fopen(path, "a") == NULL) {
            perror("smash error: open failed");
            return false;
        }
    }
    return true;
}

void Command::restoreStdOut() {
    if (stdOutCopy == -1) {
        return;
    }
    //TODO: what happens if something fails here?!
    close(1);
    dup2(stdOutCopy, 1);
    close(stdOutCopy);
}

void ChangePrompt::execute() {
    if (args[1] == NULL) {
        smallShell->setPrompt(defPrompt);
        return;
    }
    smallShell->setPrompt(args[1]);
}

void ShowPidCommand::execute() {
    cout << "smash pid is " << smashPid << endl;
}

void GetCurrDirCommand::execute() {
    char *currPath = get_current_dir_name();
    if (currPath == NULL) {
        perror("smash error: get_current_dir_name failed");
        return;
    }
    cout << currPath << endl;
    free(currPath);
}

void ChangeDirCommand::execute() {
    if (argsNum > 2) {
        cout << "smash error: cd: too many arguments" << endl;
        return;
    }
    if (args[1] == NULL) return; //got only cd without path
    if (strcmp(args[1], "-") == 0 && *lastPwd == NULL) {
        cout << "smash error: cd: OLDPWD not set" << endl;
        return;
    }
    char *currPath = get_current_dir_name();
    if (currPath == NULL) {
        perror("smash error: get_current_dir_name failed");
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
    int sigNum = 0;
    int jobId = 0;
    try {
        sigNum = stoi(args[1]) * (-1);
        jobId = stoi(args[2]);
    }
    catch (const std::exception &e) {
        cout << "smash error: kill: invalid arguments" << endl;
        return;
    }
    if (argsNum > 3 || sigNum < 0 || sigNum > 31) {
        cout << "smash error: kill: invalid arguments" << endl;
        return;
    }
    JobsList::JobEntry *toKill = jobsList->getJobById(jobId);
    if (toKill == NULL) {
        cout << "smash error: kill: job-id " << jobId << " does not "
                                                         "exist" << endl;
        return;
    }
    if (kill(toKill->getPid(), sigNum) == -1) {
        perror("smash error: kill failed");
        return;
    }
    //TODO: make sure if we need to handle special signals as SIGSTOP
    // and SIGCONT!
    if (sigNum == SIGSTOP) {
        toKill->setStatus(STOPPED);
    }
    if (sigNum == SIGCONT) { //TODO: make sure that we should do that
        toKill->setStatus(RUNNING);
    }
    cout << "signal number " << sigNum << " was sent to pid "
         << toKill->getPid() << endl;
}

void ForegroundCommand::execute() {
    int jobId = 0;
    pid_t toFGPid = 0;
    JobsList::JobEntry *toFG = NULL;
    if (argsNum > 2) {
        cout << "smash error: fg: invalid arguments" << endl;
        return;
    }
    if (argsNum == 2) { // if jobId was specified
        try {
            jobId = stoi(args[1]);
        }
        catch (const std::exception &e) { // if jobId is not a number
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
    Command *resumedCmd = toFG->getCommand();
    if (kill(toFGPid, SIGCONT) == -1) {
        perror("smash error: kill failed");
        return;
    }
    toFG->setStatus(RUNNING);
    foregroundPid = toFGPid;
    if (toFG->getCommand()->isPiped()) {
        isForegroundPipe = true;
    }
    waitpid(toFGPid, NULL, WUNTRACED);
    if (sigINTOn || sigSTPOn) { //was interrupted by signal
        handleInterruptedCmd(toFGPid, resumedCmd, toFG, jobsList);
    } else { //process finished successfully in foreground
        jobsList->removeJobById(jobId); // could also not remove and wait for removal in removeFinshedJobs
        delete resumedCmd;
        foregroundPid = 0;
        isForegroundPipe = false;
    }

}

void BackgroundCommand::execute() {
    int jobId = 0;
    pid_t toBGPid = 0;
    JobsList::JobEntry *toBG = NULL;
    if (argsNum > 2) {
        cout << "smash error: bg: invalid arguments" << endl;
        return;
    }
    if (argsNum == 2) { // if jobId was specified
        try {
            jobId = stoi(args[1]);
        }
        catch (const std::exception &e) { // if jobId is not a number
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
        if (isArgumentExist(args, "kill")) {//kill was specified
            jobsList->killAllJobs();
        }
    }
    jobsList->destroyCmds();
    smash->setToQuit(true);
}

void ExternalCommand::execute() {
    pid_t pid = fork();
    if (pid == -1) {
        perror("smash error: fork failed");
        return;
    }
    if (pid == 0) { // child process
        char **bashArgs = createBashArgs(args);
        if (isRedirected()) {
            if (!(setOutputFD(getPath(), type))) { //has to be ">" // or ">>"
                exit(0);
            }
        }
        setpgrp();
        execv("/bin/bash", bashArgs);
        perror("smash error: execv failed");
        freeBashArgs(bashArgs);
        exit(0);
    } else { // smash process
        if (isBackgroundCmd()) {//should not wait and add to jobsList
            jobsList->addJob(this, pid);
        } else {//should run in the foreground and wait for child to finish
            foregroundPid = pid;
            waitpid(pid, NULL, WUNTRACED);
            if (sigINTOn || sigSTPOn) { // was interrupted by signal
                handleInterruptedCmd(pid, this, NULL, jobsList);
            } else { // finished successfully
                delete this;
                foregroundPid = 0;
            }
        }
    }
}

void PipeCommand::execute() {
    pid_t pipePid = fork();
    if (pipePid == -1) {
        perror("smash error: fork failed");
        return;
    } //fork pipe failed
    if (pipePid == 0) {//Pipe process
        setpgrp();
        if (signal(SIGINT, pipeCtrlCHandler) == SIG_ERR) {
            perror("smash error: failed to set pipe ctrl-C handler");
            delete firstCmd;
            delete secondCmd;
            exit(0);
        }
        if (signal(SIGTSTP, pipeCtrlZHandler) == SIG_ERR) {
            perror("smash error: failed to set pipe ctrl-Z handler");
            delete firstCmd;
            delete secondCmd;
            exit(0);
        }
        if (signal(SIGCONT, pipeSigcontHandler) == SIG_ERR) {
            perror("smash error: failed to set pipe SIGCONT handler");
            delete firstCmd;
            delete secondCmd;
            exit(0);
        }
        int myPipe[2];
        pid_t sons[2] = {NOT_FORKED, NOT_FORKED};
        if (pipe(myPipe) == -1) { //creating pipe failed
            perror("smash error: pipe failed");
            return;
        }
        if (dynamic_cast<ExternalCommand *>(firstCmd) != NULL) {
            sons[0] = fork();
            if (sons[0] == -1) { //fork for firstCmd failed
                perror("smash error: fork failed");
                return;
            }
            if (sons[0] == 0) {//firstCmd
                setpgrp();
                pipeManageFD(OUT, myPipe[1], type); //close stdout of forked firstCmd and dup pipe write to stdout
                close(myPipe[0]);
                if (!(((ExternalCommand *) firstCmd)->isCp())) { //firstCmd external
                    char **firstBashArgs = createBashArgs(firstCmd->getArgs());
                    execv("/bin/bash", firstBashArgs);
                    perror("smash error: execv failed");
                    freeBashArgs(firstBashArgs);
                    exit(0);
                } else {//firstCmd is cp command
                    cpMain(firstCmd->getArgs());
                }
            } else {//Pipe process
                close(myPipe[1]); //pipe process should not be writer
                // anymore
            }
        }
        if (dynamic_cast<ExternalCommand *>(secondCmd) != NULL) {
            sons[1] = fork();
            if (sons[1] == -1) {
                perror("smash error: fork failed");
                return;
            }
            if (sons[1] == 0) {//secondCmd
                setpgrp();
                pipeManageFD(IN, myPipe[0], type); //close unused copy of pipe read
                if (!(((ExternalCommand *) secondCmd)->isCp())) { //secondCmd external
                    char **secondBashArgs = createBashArgs(secondCmd->getArgs());
                    execv("/bin/bash", secondBashArgs);
                    perror("smash error: execv failed");
                    freeBashArgs(secondBashArgs);
                    exit(0);
                } else {//secondCmd is cp command
                    cpMain(secondCmd->getArgs());
                }
            } else {//Pipe process
                close(myPipe[0]);
            }
        }
        /*from here on, only pipe process remains - externals/cp will end
        in their execv/copy stuff */
        if (sons[0] == NOT_FORKED) { // firstCmd must be built-in cmd
            int stdOutCopy = dup(1); //save a copy of stdOut
            pipeManageFD(OUT, myPipe[1], type); // close stdOut of pipe process FDT and dup pipe write to stdout
            firstCmd->execute();
            pipeManageFD(OUT, stdOutCopy, type); // close pipe write and restore stdout in the FDT
        }
        if (sons[1] == NOT_FORKED) {// secondCmd must be built in
            int stdInCopy = dup(0);
            pipeManageFD(IN, myPipe[0], type);
            secondCmd->execute();
            pipeManageFD(IN, stdInCopy, type);
        }
        pipeFirstCmdPid = sons[0];
        pipeSecondCmdPid = sons[1];
        while (wait(NULL) != -1); //TODO: make sure this waits properly to any sons existing
        if (sigINTOn) {//wait was interrupted by SIGINT, won't get here if got SIGTSTP!
            handleInterruptedCmdPipe(this);
        } else {//sons finished successfully should finish pipe as well
            delete this;
            exit(0);
        }
    } else {//Smash process
        if (isBackgroundCmd()) {//pipe runs in the background
            jobsList->addJob(this, pipePid);
        } else {//pipe runs in the foreground, wait for it and handle signals
            isForegroundPipe = true;
            foregroundPid = pipePid;
            waitpid(pipePid, NULL, WUNTRACED);
            if (sigINTOn || sigSTPOn) { // was interrupted by signal
                handleInterruptedCmd(pipePid, this, NULL, jobsList);
            } else { // finished successfully
                delete this;
                isForegroundPipe = false;
                foregroundPid = 0;
            }
        }
    }
}

void CopyCommand::execute() {
    //TODO: check if there are 3 args, cmd, and two paths
    pid_t cpPid = fork();
    if (cpPid == 0) { //cp process
        setpgrp();
        if (isRedirected()) { //TODO: check if necessary
            if (!(setOutputFD(getPath(), type))) { //has to be ">" // or ">>"
                exit(0);
            }
        }
        cpMain(args);
    } else {//smash process
        if (isBackgroundCmd()) {//should not wait and add to jobsList
            jobsList->addJob(this, cpPid);
        } else {//should run in the foreground and wait for child to finish
            foregroundPid = cpPid;
            waitpid(cpPid, NULL, WUNTRACED);
            if (sigINTOn || sigSTPOn) { // was interrupted by signal
                handleInterruptedCmd(cpPid, this, NULL, jobsList);
            } else { // finished successfully
                delete this;
                foregroundPid = 0;
            }
        }
    }

}

///Jobs list functions:

JobsList::JobsList() : maxId(0), jobsList() {
}

JobsList::JobEntry *JobsList::getJobById(int jobId) {
    for (auto iter = jobsList.begin(); iter != jobsList.end();
         ++iter) {
        if (iter->getJobId() == jobId) {
            return &(*iter);
        }
    }
    return NULL;
}

JobsList::JobEntry *JobsList::getLastJob(int *lastJobId) {
    if (jobsList.empty()) {
        *lastJobId = 0;
        return NULL;
    }
    *lastJobId = jobsList.back().getJobId();
    return &(jobsList.back());
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
        int elapsedTime = difftime(currTime, iter->getStartTime());
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

JobsList::JobEntry *JobsList::getLastStoppedJob(int *jobId) {
    JobsList::JobEntry *maxStoppedJob = NULL;
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
    cout << "smash: sending SIGKILL signal to " << jobsList.size()
         << " jobs:" << endl;
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

void JobsList::addJob(Command *cmd, pid_t pid, bool isStopped) {
    removeFinishedJobs();
    STATUS status = isStopped ? STOPPED : RUNNING;
    JobEntry toAdd(++maxId, pid, cmd, status);
    jobsList.push_back(toAdd);
}

///Smash functions:

SmallShell::SmallShell() : prompt(defPrompt), lastPwd(NULL),
                           jobsList(), toQuit(false) {
}

SmallShell::~SmallShell() {
    free(lastPwd);
}

Command *SmallShell::CreateCommand(const char *cmd_line) {
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
        return new ShowPidCommand(cmd_line, getpid());
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
    if (cmdOnly == "cp") {
        return new CopyCommand(cmd_line, &jobsList);
    } else { // External Cmds
        return new ExternalCommand(cmd_line, &jobsList);
    }
}

void SmallShell::executeCommand(const char *cmd_line) {
    bool isBuiltIn = false, redirectedSuccess = true;
    if (_trim(cmd_line).empty()) { //no cmd received, go get next cmd
        return;
    }     //TODO: ask if that what we should do in the piazza!
    Command *cmd = CreateCommand(cmd_line);
    jobsList.removeFinishedJobs();
    IO_CHARS cmdIOType = cmd->getType();
    if (cmd->isPiped()) { //prepare sub cmds of the pipe
        string stringCmd = (string) (cmd_line);
        unsigned long pipeIndex = stringCmd.find('|');
        string firstCmd = stringCmd.substr(0, pipeIndex);
        string secondCmd;
        if (cmdIOType == PIPE) { //TODO: verify staring location of
            // second cmd , is it really pipeIndex + 1?
            secondCmd = stringCmd.substr(pipeIndex + 1);
        } else {//must be PIPE_ERR //TODO: verify also here (|&)
            secondCmd = stringCmd.substr(pipeIndex + 2);
        }
        ((PipeCommand *) (cmd))->setFirstCmd( //converting to PipeCommand
                // to access setCmds member functions
                CreateCommand(firstCmd.c_str()));
        ((PipeCommand *) (cmd))->setSecondCmd(
                CreateCommand(secondCmd.c_str()));
    }
    if (dynamic_cast<BuiltInCommand *>(cmd) != NULL) {
        isBuiltIn = true;
        if (cmd->isRedirected()) { // redirect stdout to the
            // requested file
            if (!(cmd->setOutputFD(cmd->getPath(), cmdIOType))) {
                redirectedSuccess = false;
            }
        }
    }
    if (dynamic_cast<BuiltInCommand *>(cmd) != NULL) { // if built in
        if (redirectedSuccess) { //execute if not redirected (because set to true by default) or
            // if redirected cmd and was redirect successfully
            cmd->execute();
        }
    } else { //not built in so execute anyhow
        cmd->execute();
    }
    if (isBuiltIn) {
        if (cmd->isRedirected()) { //restore stdout to channel 1 in FDT
            cmd->restoreStdOut();
        }
        delete cmd;
    }
}
