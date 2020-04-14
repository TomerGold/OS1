#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <list>
#include <cstring>
#include <fstream>

using std::ostream;

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

using std::string;
using std::list;

bool sigSTPOn = false;
bool sigINTOn = false;

typedef enum {
    RUNNING, STOPPED
} STATUS;
typedef enum {
    REDIR, REDIR_APPEND, PIPE, PIPE_ERR, NONE
} IO_CHARS;

class Command {
protected:
    string origCmd;
    char* args[COMMAND_MAX_ARGS + 1]{};
    int argsNum;
    bool isBackground;
    pid_t pid;
    bool redirected;
    bool piped;
    IO_CHARS type;
    int stdOutCopy;
public:
    explicit Command(const char* cmd_line);

    virtual ~Command() {
        for (int i = 0; i < COMMAND_MAX_ARGS + 1; i++) {
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

    pid_t getPid() const {
        return pid;
    }

    IO_CHARS containsSpecialChars() const;

    void setOutputFD(const char* path, IO_CHARS type);

    bool isRedirected() const {
        return redirected;
    }

    int getStdOutCopy() const {
        return stdOutCopy;
    }

    const char* getPath() const {
        return args[argsNum - 1];
    }

    IO_CHARS getType() const {
        return type;
    }

    bool isPiped() const {
        return piped;
    }

    void restoreStdOut();

    //virtual void prepare();
    //virtual void cleanup();
    // TODO: Add your extra methods if needed
};

class BuiltInCommand : public Command {
public:
    explicit BuiltInCommand(const char* cmd_line) : Command(cmd_line) {
    };

    virtual ~BuiltInCommand() = default;
};

class ChangeDirCommand : public BuiltInCommand {
    char** lastPwd;
public:
    ChangeDirCommand(const char* cmd_line, char** plastPwd) :
            BuiltInCommand(cmd_line), lastPwd(plastPwd) {
    };

    virtual ~ChangeDirCommand() = default;

    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    explicit GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(
            cmd_line) {
    };

    virtual ~GetCurrDirCommand() = default;

    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    explicit ShowPidCommand(const char* cmd_line) : BuiltInCommand(
            cmd_line) {
    };

    virtual ~ShowPidCommand() = default;

    void execute() override;
};

class JobsList {
public:
    class JobEntry {
        int jobId;
        pid_t pid;
        Command* cmd;
        STATUS status;
        time_t startTime;

    public:

        JobEntry(int jobId, int pid, Command* cmd, STATUS status) :
                jobId(jobId), pid(pid), cmd(cmd), status(status),
                startTime((time)(NULL)) {
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

        Command* getCommand() const {
            return cmd;
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
    };

private:
    int maxId;

    list<JobEntry> jobsList;

public:
    JobsList();

    ~JobsList() = default;

    void addJob(Command* cmd, bool isStopped = false);

    void printJobsList();

    void killAllJobs();

    void removeFinishedJobs();

    JobEntry* getJobById(int jobId);

    void removeJobById(int jobId);

    JobEntry* getLastJob(int* lastJobId);

    JobEntry* getLastStoppedJob(int* jobId);

    bool isJobListEmpty() const {
        return jobsList.empty();
    }

    bool stoppedJobExists() const;

    void destroyCmds();
};

class ExternalCommand : public Command {
    JobsList* jobsList;
public:
    ExternalCommand(const char* cmd_line, JobsList*
    jobs) : Command(cmd_line), jobsList(jobs) {
    };

    virtual ~ExternalCommand() = default;

    void execute() override;
};

class PipeCommand : public Command {
    Command* firstCmd;
    Command* secondCmd;
    JobsList* jobsList;
public:
    explicit PipeCommand(const char* cmd_line, JobsList* jobsList) :
            Command(cmd_line), firstCmd(NULL), secondCmd(NULL),
            jobsList(jobsList) {
    };

    virtual ~PipeCommand() {

    }

    void setFirstCmd(Command* first) {
        firstCmd = first;
    }

    void setSecondCmd(Command* second) {
        secondCmd = second;
    }

    void execute() override;
};

class JobsCommand : public BuiltInCommand {
    JobsList* jobsList;
public:
    JobsCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand
                                                                (cmd_line),
                                                        jobsList(jobs) {
    };

    virtual ~JobsCommand() = default;

    void execute() override;
};

class KillCommand : public BuiltInCommand {
    JobsList* jobsList;
public:
    KillCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(
            cmd_line), jobsList(jobs) {
    };

    virtual ~KillCommand() = default;

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    JobsList* jobsList;
public:
    ForegroundCommand(const char* cmd_line, JobsList* jobs)
            : BuiltInCommand
                      (cmd_line),
              jobsList(jobs) {
    };

    virtual ~ForegroundCommand() = default;

    void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
    JobsList* jobsList;
public:
    BackgroundCommand(const char* cmd_line, JobsList* jobs)
            : BuiltInCommand
                      (cmd_line),
              jobsList(jobs) {
    };

    virtual ~BackgroundCommand() = default;

    void execute() override;
};

// TODO: should it really inhirit from BuiltInCommand ?
class CopyCommand : public BuiltInCommand {

public:
    CopyCommand(const char* cmd_line);

    virtual ~CopyCommand() {
    }

    void execute() override;
};

// TODO: add more classes if needed maybe timeout ?
//

class SmallShell {
private:
    // TODO: Add your data members
    SmallShell();

    string prompt;

    char* lastPwd;

    JobsList jobsList;

    bool toQuit;

public:
    Command* CreateCommand(const char* cmd_line);

    SmallShell(SmallShell const&) = delete; // disable copy ctor
    void operator=(SmallShell const&) = delete; // disable = operator
    static SmallShell& getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    string getPrompt() const {
        return prompt;
    }

    void setPrompt(const string& newPrompt) {
        prompt = newPrompt;
    }

    bool getToQuit() const {
        return toQuit;
    }

    void setToQuit(bool quit) {
        toQuit = quit;
    }

    ~SmallShell();

    void executeCommand(const char* cmd_line);
    // TODO: add extra methods as needed
};

class ChangePrompt : public BuiltInCommand {
    SmallShell* smallShell;
public:
    ChangePrompt(const char* cmd_line, SmallShell* smash) :
            BuiltInCommand(cmd_line), smallShell(smash) {
    };

    virtual ~ChangePrompt() = default;

    void execute() override;
};

class QuitCommand : public BuiltInCommand {
    JobsList* jobsList;
    SmallShell* smash;
public:
    QuitCommand(const char* cmd_line, JobsList* jobs, SmallShell* smash) :
            BuiltInCommand
                    (cmd_line),
            jobsList(jobs),
            smash(smash) {
    };

    virtual ~QuitCommand() = default;

    void execute() override;
};

#endif //SMASH_COMMAND_H_

//TODO: consider deleting this class
//class RedirectionCommand : public Command {
//    // TODO: Add your data members
//public:
//    explicit RedirectionCommand(const char* cmd_line);
//
//    virtual ~RedirectionCommand() {
//    }
//
//    void execute() override;
//    //void prepare() override;
//    //void cleanup() override;
//};

//class CommandsHistory {
//protected:
//    class CommandHistoryEntry {
//        // TODO: Add your data members
//    };
//    // TODO: Add your data members
//public:
//    CommandsHistory();
//    ~CommandsHistory() {}
//    void addRecord(const char* cmd_line);
//    void printHistory();
//};
//
//class HistoryCommand : public BuiltInCommand {
//    // TODO: Add your data members
//public:
//    HistoryCommand(const char* cmd_line, CommandsHistory* history);
//    virtual ~HistoryCommand() {}
//    void execute() override;
//};