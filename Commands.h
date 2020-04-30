#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <list>
#include <cstring>
#include <fstream>
#include <unistd.h>

using std::ostream;

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define NOT_FORKED (-2)
#define NO_NEXT_ALARM (-2)
#define ARGS_AMOUNT (25)

using std::string;
using std::list;

extern bool sigSTPOn;
extern bool sigINTOn;
extern bool sigAlarmOn;
extern pid_t foregroundPid;
extern bool isForegroundPipe;
extern string defPrompt;
extern pid_t pipeFirstCmdPid;
extern pid_t pipeSecondCmdPid;
extern bool isForegroundTimeout;
extern pid_t timeoutInnerCmdPid;
extern time_t nextEndingTime;


typedef enum {
    RUNNING, STOPPED
} STATUS;
typedef enum {
    REDIR, REDIR_APPEND, PIPE, PIPE_ERR, NONE
} IO_CHARS;
typedef enum {
    IN, OUT
} FDT_CHANNEL;

class Command {
protected:
    bool isBackground;
    string origCmd;
    char *args[ARGS_AMOUNT];
    int argsNum;
    bool redirected;
    bool piped;
    IO_CHARS type;
    int stdOutCopy;
    bool isTimeout;
    bool finishedBeforeTimeout;
public:
    explicit Command(const char *cmd_line);

    virtual ~Command() {
        for (int i = 0; i < ARGS_AMOUNT; i++) {
            if (args[i] != NULL) {
                free(args[i]);
            } else {
                break;
            }
        }
    }

    string getOrigCmd() const {
        return origCmd;
    }

    bool isBackgroundCmd() const {
        return isBackground;
    }

    virtual void execute() = 0;

    IO_CHARS containsSpecialChars() const;

    bool setOutputFD(const char *path, IO_CHARS type);

    bool isRedirected() const {
        return redirected;
    }

    int getArgsNum() const {
        return argsNum;
    }

    const char *getPath() const {
        int i = 0;
        while (args[i] != NULL) {
            if (strcmp(args[i], ">") == 0) {
                return args[i + 1];
            } else if (strcmp(args[i], ">>") == 0) {
                return args[i + 1];
            }
            i++;
        }
        return NULL;
    }

    IO_CHARS getType() const {
        return type;
    }

    bool isPiped() const {
        return piped;
    }

    void restoreStdOut();

    bool isTimeouted() const {
        return isTimeout;
    }

    char *const *getArgs() const {
        return args;
    }

    void setFinishedBeforeTimeoutTrue() {
        finishedBeforeTimeout = true;
    }

    bool isFinishedBeforeTimeout() const {
        return finishedBeforeTimeout;
    }
};

class BuiltInCommand : public Command {
public:
    explicit BuiltInCommand(const char *cmd_line) : Command(cmd_line) {
    };

    virtual ~BuiltInCommand() = default;
};

class ChangeDirCommand : public BuiltInCommand {
    char **lastPwd;
public:
    ChangeDirCommand(const char *cmd_line, char **plastPwd) :
            BuiltInCommand(cmd_line), lastPwd(plastPwd) {
    };

    virtual ~ChangeDirCommand() = default;

    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    explicit GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(
            cmd_line) {
    };

    virtual ~GetCurrDirCommand() = default;

    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
    pid_t smashPid;
public:
    ShowPidCommand(const char *cmd_line, pid_t smashPid) : BuiltInCommand(
            cmd_line), smashPid(smashPid) {
    };

    virtual ~ShowPidCommand() = default;

    void execute() override;
};

class JobsList {
public:
    class JobEntry {
        int jobId;
        pid_t pid;
        Command *cmd;
        STATUS status;
        time_t startTime;

    public:

        JobEntry(int jobId, int pid, Command *cmd, STATUS status) :
                jobId(jobId), pid(pid), cmd(cmd), status(status) {
            startTime = time(NULL);
            if (startTime == (time_t) (-1)) {
                perror("smash error: time failed");
            }
        };

        ~JobEntry() = default;

        int getJobId() const {
            return jobId;
        }

        pid_t getPid() const {
            return pid;
        }

        Command *getCommand() const {
            return cmd;
        }

        void setCommandToNull() {
            cmd = NULL;
        }

        STATUS getStatus() const {
            return status;
        }

        void setStatus(STATUS newStatus) {
            status = newStatus;
        }

        time_t getStartTime() const {
            return startTime;
        }

        void setStartTimeNow() {
            startTime = time(NULL);
            if (startTime == (time_t) (-1)) {
                perror("smash error: time failed");
            }
        }

        bool operator==(const JobEntry &entry) {
            return jobId == entry.jobId;
        }
    };

private:
    int maxId;

    list<JobEntry> jobsList;

public:
    JobsList();

    ~JobsList() = default;

    void addJob(Command *cmd, pid_t pid, bool isStopped = false);

    void printJobsList();

    void killAllJobs();

    void removeFinishedJobs();

    JobEntry *getJobById(int jobId);

    void removeJobById(int jobId);

    JobEntry *getLastJob(int *lastJobId);

    JobEntry *getLastStoppedJob(int *jobId);

    bool isJobListEmpty() const {
        return jobsList.empty();
    }

    bool stoppedJobExists() const;

    void destroyCmds();

    int getMaxId() const {
        return maxId;
    }

    JobEntry *getJobByPid(pid_t pid);

    JobEntry *getSoonestTimeoutEntry();
};

class ExternalCommand : public Command {
protected:
    JobsList *jobsList;
    bool isCpCmd;
public:
    ExternalCommand(const char *cmd_line, JobsList *
    jobs, bool isCpCmd = false) : Command(cmd_line), jobsList(jobs),
                                  isCpCmd(isCpCmd) {
    };

    virtual ~ExternalCommand() = default;

    void execute() override;

    bool isCp() const {
        return isCpCmd;
    }
};

class PipeCommand : public Command {
    Command *firstCmd;
    Command *secondCmd;
    JobsList *jobsList;
public:
    PipeCommand(const char *cmd_line, JobsList *jobsList) :
            Command(cmd_line), firstCmd(NULL), secondCmd(NULL),
            jobsList(jobsList) {
    };

    virtual ~PipeCommand() {
        delete firstCmd;
        delete secondCmd;
    }

    void setFirstCmd(Command *first) {
        firstCmd = first;
    }

    void setSecondCmd(Command *second) {
        secondCmd = second;
    }

    void execute() override;
};

class JobsCommand : public BuiltInCommand {
    JobsList *jobsList;
public:
    JobsCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand
                                                                (cmd_line),
                                                        jobsList(jobs) {
    };

    virtual ~JobsCommand() = default;

    void execute() override;
};

class KillCommand : public BuiltInCommand {
    JobsList *jobsList;
public:
    KillCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(
            cmd_line), jobsList(jobs) {
    };

    virtual ~KillCommand() = default;

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    JobsList *jobsList;
public:
    ForegroundCommand(const char *cmd_line, JobsList *jobs)
            : BuiltInCommand
                      (cmd_line),
              jobsList(jobs) {
    };

    virtual ~ForegroundCommand() = default;

    void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
    JobsList *jobsList;
public:
    BackgroundCommand(const char *cmd_line, JobsList *jobs)
            : BuiltInCommand
                      (cmd_line),
              jobsList(jobs) {
    };

    virtual ~BackgroundCommand() = default;

    void execute() override;
};

class CopyCommand : public ExternalCommand {
public:
    CopyCommand(const char *cmd_line, JobsList *jobsList) :
            ExternalCommand(cmd_line, jobsList, true) {
    };

    virtual ~CopyCommand() = default;

    void execute() override;
};

class TimeoutCommand : public Command {
    Command *innerCmd;
    JobsList *jobsList;
    int duration;
    time_t endingTime;

public:
    TimeoutCommand(const char *cmd_line, JobsList *jobsList) :
            Command(cmd_line), innerCmd(NULL), jobsList(jobsList), duration(0), endingTime(0) {

    };

    time_t getEndingTime() const {
        return endingTime;
    }

    virtual ~TimeoutCommand() {
        delete innerCmd;
    }

    void setInnerCmd(Command *inner) {
        innerCmd = inner;
    }

    void execute() override;
};

class SmallShell {
private:
    SmallShell();

    string prompt;

    char *lastPwd;

    JobsList jobsList;

    bool toQuit;

public:
    Command *CreateCommand(const char *cmd_line);

    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    string getPrompt() const {
        return prompt;
    }

    void setPrompt(const string &newPrompt) {
        prompt = newPrompt;
    }

    bool getToQuit() const {
        return toQuit;
    }

    void setToQuit(bool quit) {
        toQuit = quit;
    }

    ~SmallShell();

    void executeCommand(const char *cmd_line);
};

class ChangePrompt : public BuiltInCommand {
    SmallShell *smallShell;
public:
    ChangePrompt(const char *cmd_line, SmallShell *smash) :
            BuiltInCommand(cmd_line), smallShell(smash) {
    };

    virtual ~ChangePrompt() = default;

    void execute() override;
};

class QuitCommand : public BuiltInCommand {
    JobsList *jobsList;
    SmallShell *smash;
public:
    QuitCommand(const char *cmd_line, JobsList *jobs, SmallShell *smash) :
            BuiltInCommand
                    (cmd_line),
            jobsList(jobs),
            smash(smash) {
    };

    virtual ~QuitCommand() = default;

    void execute() override;
};

extern JobsList alarmList;
extern pid_t nextAlarmedPid;

void removeTimeoutAndSetNewAlarm(pid_t finishedPid);

#endif //SMASH_COMMAND_H_
