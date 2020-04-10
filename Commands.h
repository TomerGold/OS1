#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <list>
#include <cstring>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define HISTORY_MAX_RECORDS (50)

using std::string;
using std::list;


typedef enum {
    RUNNING, STOPPED
} STATUS;

class Command {
// TODO: Add your data members (args...)
protected:
    string origCmd;
    char* args[COMMAND_MAX_ARGS + 1];
    int argsNum;
    bool isBackground;
public:
    Command(const char* cmd_line);

    virtual ~Command();

    string getOrigCmd() const {
        return origCmd;
    }

    virtual void execute() = 0;
    //virtual void prepare();
    //virtual void cleanup();
    // TODO: Add your extra methods if needed
};

class BuiltInCommand : public Command {
public:
    BuiltInCommand(const char* cmd_line);

    virtual ~BuiltInCommand() {
    }
};

class ExternalCommand : public Command {
public:
    ExternalCommand(const char* cmd_line);

    virtual ~ExternalCommand() {
    }

    void execute() override;
};

class PipeCommand : public Command {
    // TODO: Add your data members
public:
    PipeCommand(const char* cmd_line);

    virtual ~PipeCommand() {
    }

    void execute() override;
};

class RedirectionCommand : public Command {
    // TODO: Add your data members
public:
    explicit RedirectionCommand(const char* cmd_line);

    virtual ~RedirectionCommand() {
    }

    void execute() override;
    //void prepare() override;
    //void cleanup() override;
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
    GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {
    };

    virtual ~GetCurrDirCommand() = default;

    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {
    };

    virtual ~ShowPidCommand() = default;

    void execute() override;
};

class JobsList;

class QuitCommand : public BuiltInCommand {
// TODO: Add your data members public:
    QuitCommand(const char* cmd_line, JobsList* jobs);

    virtual ~QuitCommand() {
    }

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
                startTime(time(NULL)) {
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

    // TODO: Add extra methods or modify exisitng ones as needed
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
    KillCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand
                                                                (cmd_line),
                                                        jobsList(jobs) {
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
    // TODO: Add your data members
public:
    BackgroundCommand(const char* cmd_line, JobsList* jobs);

    virtual ~BackgroundCommand() {
    }

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

// TODO: add more classes if needed
// maybe timeout ?

class SmallShell {
private:
    // TODO: Add your data members
    SmallShell();

    string prompt;

    char* lastPwd;

    JobsList jobsList;

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

    ~SmallShell();

    void executeCommand(const char* cmd_line);
    // TODO: add extra methods as needed
};

class ChangePrompt : public BuiltInCommand {
    SmallShell* s;
public:
    ChangePrompt(const char* cmd_line, SmallShell* s) :
            BuiltInCommand(cmd_line), s(s) {
    };

    virtual ~ChangePrompt() = default;

    void execute() override;
};

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

#endif //SMASH_COMMAND_H_
