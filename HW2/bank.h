#ifndef BANK_H
#define BANK_H

#include <map>
#include <string>
#include <pthread.h>
#include "account.h"
#include "atm.h"
#include <vector>
#include <memory>


class Bank {
public:
    pthread_mutex_t atm_mutex;      // lock the atm
    std::vector<ATM*> atms;

    Bank();
    ~Bank();

    // Action called by ATM
    bool openAccount(int id, const std::string& password, int balance, int atm_id);
    bool deposit(int id, const std::string& password, int amount, int atm_id);
    bool withdraw(int id, const std::string& password, int amount, int atm_id);
    bool balanceInquiry(int id, const std::string& password, int atm_id);
    bool closeAccount(int id, const std::string& password, int atm_id);
    bool transfer(int src_id, const std::string& password, int dst_id, int amount, int atm_id);
    bool closeATM(int src_id, int dst_id);
    bool closeAtmRequest(int src_id, int dst_id);

    // doing commisions and status printing in background
    void startBankThreads(); 
        void logToFile(const std::string& msg);


private:	
	std::map<int, std::shared_ptr<Account>> accounts;
    pthread_mutex_t bank_mutex;    // lock the accounts map
    pthread_mutex_t log_mutex;     // lock the log file
    
    pthread_t commissionThread;
    pthread_t statusThread;
    bool stopFlag;

    static void* commissionRoutine(void* arg);
    static void* statusRoutine(void* arg);


};

#endif
