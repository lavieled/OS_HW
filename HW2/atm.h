#ifndef ATM_H
#define ATM_H

#include <string>
#include <pthread.h>
#include <atomic>
// forward decleration
class Bank;

class ATM {
public:
    ATM(int atm_id, const std::string& filename, Bank* bank);
    ~ATM();
    void start(); // start thread
    void join();  // wait for finish
    void setStopFlag();
    bool getStopFlag();
    int  getAtmId();

	void markForClosure() { markedForClosure = true; }// mark to close at next status
	bool isMarkedForClosure() const { return markedForClosure.load(); }//check if supposed to close

private:
	
    int atm_id;
    std::string filename;
    Bank* bank;
    bool stopFlag;//atm stopped
	std::atomic<bool> markedForClosure;
    pthread_t thread;

    static void* runHelper(void* arg);
    void run();
};

#endif
