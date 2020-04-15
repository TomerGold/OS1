#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

bool sigSTPOn = false;
bool sigINTOn = false;

void ctrlCHandler(int sig_num) {
    cout << "smash: got ctrl-C" << endl;
    //TODO: if there is no process in the foreground stop here!

    //TODO: for pipes - don't send SIGKILL, send SIGINT and pipe
    // handler will stop it's sons and then kill pipe, after that
    // rememeber to set isForegroundPipe to false after handling
    kill(foregroundPid, SIGKILL);
    cout << "smash: process " << foregroundPid << " was killed" << endl;
}

void ctrlZHandler(int sig_num) {
    cout << "smash: got ctrl-Z" << endl;
    //TODO: if there is no process in the foreground stop here!

    //TODO: for pipes - don't send SIGSTOP, send SIGSTP and pipe
    // handler will stop it's sons and then STOP pipe, after that
    // rememeber to set isForegroundPipe to false after handling
    kill(foregroundPid, SIGSTOP);
    cout << "smash: process " << foregroundPid << " was stopped" << endl;
}

//TODO: do we need a handler to SIGCONT for a pipe?

void alarmHandler(int sig_num) {
    // TODO: Add your implementation
}

