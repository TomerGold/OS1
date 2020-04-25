#ifndef SMASH__SIGNALS_H_
#define SMASH__SIGNALS_H_

void ctrlZHandler(int sig_num);

void ctrlCHandler(int sig_num);

void alarmHandler(int sig_num);

void pipeCtrlCHandler(int sig_num);

void pipeCtrlZHandler(int sig_num);

void pipeSigcontHandler(int sig_num);

void timeoutCtrlCHandler(int sig_num);

void timeoutCtrlZHandler(int sig_num);

void timeoutSigcontHandler(int sig_num);

#endif //SMASH__SIGNALS_H_
