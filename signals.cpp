#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"
#include <unistd.h>


using namespace std;

bool sigSTPOn = false;
bool sigINTOn = false;
bool sigAlarmOn = false;

void ctrlCHandler(int sig_num) {
    cout << "smash: got ctrl-C" << endl;
    if (foregroundPid == 0) {
        return;
    }
    sigINTOn = true;
    if (isForegroundTimeout) {
        kill(foregroundPid, SIGINT);
    }
    if (isForegroundPipe) {
        kill(foregroundPid, SIGINT);
    } else {
        kill(foregroundPid, SIGKILL);
    }
    cout << "smash: process " << foregroundPid << " was killed" << endl;
}

void ctrlZHandler(int sig_num) {
    cout << "smash: got ctrl-Z" << endl;
    if (foregroundPid == 0) {
        return;
    }
    sigSTPOn = true;
    if (isForegroundTimeout) {
        kill(foregroundPid, SIGINT);
    }
    if (isForegroundPipe) {
        kill(foregroundPid, SIGTSTP);
    } else {
        kill(foregroundPid, SIGSTOP);
    }
    cout << "smash: process " << foregroundPid << " was stopped" << endl;
}

void pipeCtrlCHandler(int sig_num){
    sigINTOn = true; //pay attention, this is copy of the global sigSTPOn of the smash process, it's the same one!!
    if (pipeFirstCmdPid != NOT_FORKED){
        kill(pipeFirstCmdPid, SIGKILL);
    }
    if(pipeSecondCmdPid != NOT_FORKED){
        kill(pipeSecondCmdPid, SIGKILL);
    }
}

void pipeCtrlZHandler(int sig_num){
    if (pipeFirstCmdPid != NOT_FORKED){
        kill(pipeFirstCmdPid, SIGSTOP);
    }
    if(pipeSecondCmdPid != NOT_FORKED){
        kill(pipeSecondCmdPid, SIGSTOP);
    }
    kill(getpid(), SIGSTOP);
}

void pipeSigcontHandler(int sig_num){
    if (pipeFirstCmdPid != NOT_FORKED){
        kill(pipeFirstCmdPid, SIGCONT);
    }
    if(pipeSecondCmdPid != NOT_FORKED){
        kill(pipeSecondCmdPid, SIGCONT);
    }

    /*after this, pipe should restart the waitpid in the pipe execute
     * to wait for his sons that have been already continued until they finish
     * or until the next signal interrupt*/
}

void alarmHandler(int sig_num) {
    //TODO: add printings!
    if (foregroundPid == nextAlarmedPid) {
        sigAlarmOn = true;
    }
    JobsList::JobEntry *toRemoveJob = alarmList.getJobByPid(nextAlarmedPid);
    delete (toRemoveJob->getCommand());
    alarmList.removeJobById(toRemoveJob->getJobId()); //TODO: weak point
    kill(nextAlarmedPid, SIGINT); //sending SIGINT so Timeout cmd will kill it's inner cmd and commit suicide
    JobsList::JobEntry *soonest = alarmList.getSoonestTimeoutEntry();
    if (soonest != NULL) {
        alarm(nextEndingTime - time(NULL));
        nextAlarmedPid = soonest->getPid();
    } else {
        alarm(0); // cancel further alarms since there are no more timeout cmds
    }
}

//TODO: add timeoutCtrlC handler
//TODO: add timeoutCtrlZ handler
//TODO: add timeoutSigcontHandler

