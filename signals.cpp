#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) {

    cout << "smash: got ctrl-Z" << endl;
    sigSTPOn = true;
    // TODO: Add your implementation
}

void ctrlCHandler(int sig_num) {
    cout << "smash: got ctrl-C" << endl;
    sigINTOn = true;
}

void alarmHandler(int sig_num) {
    // TODO: Add your implementation
}

