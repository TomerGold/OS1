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

Command::Command(const char* cmd_line) {
    argsNum = _parseCommandLine(cmd_line, args);
    //TODO: handle jobId
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

void ChangeDirCommand::execute() {
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

SmallShell::SmallShell() : prompt(DEFAULT_PROMPT), lastPwd(NULL) {
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
