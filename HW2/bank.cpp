#include "bank.h"
#include <iostream>
#include <fstream>
#include <unistd.h> // for sleep
#include <ctime>
#include <algorithm>

extern std::ofstream logFile;

Bank::Bank() : stopFlag(false) {
	pthread_mutex_init(&bank_mutex, nullptr);//bank lock
	pthread_mutex_init(&log_mutex, nullptr);//log lock
	pthread_mutex_init(&atm_mutex, nullptr);//atm lock
	accounts[0] = std::make_shared<Account>(0, "0000", 0);//make the banks comission account
}

Bank::~Bank() {

	/*
	stopFlag = true;
	pthread_mutex_destroy(&bank_mutex);
	pthread_mutex_destroy(&log_mutex);
	pthread_mutex_destroy(&atm_mutex);
	pthread_join(commissionThread, nullptr);
	pthread_join(statusThread, nullptr);
	*/
    stopFlag = true;
    pthread_join(commissionThread, nullptr);
    pthread_join(statusThread, nullptr);

    for (ATM* atm : atms) {
        delete atm;
    }

    pthread_mutex_destroy(&bank_mutex);
    pthread_mutex_destroy(&log_mutex);
    pthread_mutex_destroy(&atm_mutex);
}

void Bank::startBankThreads() {
    pthread_create(&statusThread, nullptr, &Bank::statusRoutine, this);
    pthread_create(&commissionThread, nullptr, &Bank::commissionRoutine, this);
}


void* Bank::statusRoutine(void* arg) {
    Bank* bank = static_cast<Bank*>(arg);
    while (!bank->stopFlag) {
        usleep(500 * 1000); // 0.5 secs

       std::vector<Account*> sorted_accounts;

	//copy pointers with lock
	pthread_mutex_lock(&bank->bank_mutex);
	for (const auto& p : bank->accounts) {
	    sorted_accounts.push_back(p.second.get());
	}
	pthread_mutex_unlock(&bank->bank_mutex);

	//sort by account ID to ensure consistent lock order
	std::sort(sorted_accounts.begin(), sorted_accounts.end(),
		  [](Account* a, Account* b) {
		      return a->getId() < b->getId();
		  });
	// print to screen with display commands
	printf("\033[2J");
	printf("\033[1;1H");
	printf("Current Bank Status\n");
	for (Account* acc : sorted_accounts) {
	    // internally lock their own lock in consistent order
	    printf("Account %d: Balance - %d $, Account Password - %s\n",
		   acc->getId(), acc->getBalance(), acc->getPassword().c_str());
	}
	pthread_mutex_lock(&bank->atm_mutex);	
	for (ATM* atm : bank->atms) {
	    if (atm->isMarkedForClosure()) {
		atm->setStopFlag();  // actually shut down the ATM now
	    }
}
pthread_mutex_unlock(&bank->atm_mutex);
    }
    return nullptr;
}

void* Bank::commissionRoutine(void* arg) {
    Bank* bank = static_cast<Bank*>(arg);
    srand(time(nullptr));

    while (!bank->stopFlag) {
        sleep(3); // sleep 3 sec

        int percent = (rand() % 5) + 1; // pick rand 1-5% for comission

        pthread_mutex_lock(&bank->bank_mutex);
        for (auto& p : bank->accounts) {
            Account* acc = p.second.get();
            if (acc->id == 0) continue; // 0 is the bank's account
            int fee = (acc->balance * percent) / 100;
            acc->withdraw(fee);
            bank->accounts[0]->deposit(fee);

            bank->logToFile("Bank: commissions of " + std::to_string(percent) + "% were charged, bank gained " +
                             std::to_string(fee) + " from account " + std::to_string(acc->getId()));
        }
        pthread_mutex_unlock(&bank->bank_mutex);
    }
    return nullptr;
}

bool Bank::openAccount(int id, const std::string& password, int balance, int atm_id) {
    pthread_mutex_lock(&bank_mutex);

	if ((id < 0) || (password.length() < 4 )|| (balance < 0)) {
        pthread_mutex_unlock(&bank_mutex);
        logToFile("Error " + std::to_string(atm_id) + ": Your transaction failed – illegal account parameters");
        return false;
    }
    if (accounts.find(id) != accounts.end()) { // account already exist
        pthread_mutex_unlock(&bank_mutex);
        logToFile("Error " + std::to_string(atm_id) +
                  ": Your transaction failed – account with the same id exists");
        return false;
    }

    accounts[id] = std::make_shared<Account>(id, password, balance); // new Account(id, password, balance);
    pthread_mutex_unlock(&bank_mutex);

    logToFile(std::to_string(atm_id) + ": New account id is " + std::to_string(id) +
              " with password " + password + " and initial balance " + std::to_string(balance));
    return true;
}
bool Bank::deposit(int id, const std::string& password, int amount, int atm_id){
    pthread_mutex_lock(&bank_mutex);
    auto it = accounts.find(id);
    if (it == accounts.end()) { // no account found
        pthread_mutex_unlock(&bank_mutex);
        logToFile("Error " + std::to_string(atm_id) + ": Your transaction failed - account id " +
            std::to_string(id) + " does not exist");
        return false;
    }
	Account* acc = it->second.get();   
    pthread_mutex_unlock(&bank_mutex);
    if(!acc->getAccess(password)){ // wrong password
        logToFile("Error " + std::to_string(atm_id) +
                  ": Your transaction failed – password for account id " + std::to_string(id) + " is incorrect");
        return false;
    }
    int new_balance = acc->deposit(amount);
    logToFile(std::to_string(atm_id) + ": Account " + std::to_string(id) + " new balance is "+ 
        std::to_string(new_balance)+ " after " + std::to_string(amount) + "$ was deposited");
    return true;

}
bool Bank::withdraw(int id, const std::string& password, int amount, int atm_id){
    pthread_mutex_lock(&bank_mutex);
    auto it = accounts.find(id);
    if (it == accounts.end()) { // no account found
        pthread_mutex_unlock(&bank_mutex);
        logToFile("Error " + std::to_string(atm_id) + ": Your transaction failed - account id " +
            std::to_string(id) + " does not exist");
        return false;
    }
	Account* acc = it->second.get();   
    pthread_mutex_unlock(&bank_mutex);
    if(!acc->getAccess(password)){ // wrong password
        logToFile("Error " + std::to_string(atm_id) +
                  ": Your transaction failed – password for account id " + std::to_string(id) + " is incorrect");
        return false;
    }
    int new_balance = 0;
    if((new_balance = acc->withdraw(amount)) < 0){ // not enough money
        logToFile("Error " + std::to_string(atm_id) + ": Your transaction failed – account id "+
                                 std::to_string(id) + " balance is lower than " + std::to_string(amount));
        return false;
    }
    logToFile(std::to_string(atm_id) + ": Account " + std::to_string(id)+ " new balance is "+ 
        std::to_string(new_balance)+ " after "+ std::to_string(amount) + "$ was withdrawn");
    return true;
}
bool Bank::balanceInquiry(int id, const std::string& password, int atm_id){
     pthread_mutex_lock(&bank_mutex);
     auto it = accounts.find(id);
    if (it == accounts.end()) { // no account found
        pthread_mutex_unlock(&bank_mutex);
        logToFile("Error " + std::to_string(atm_id) + ": Your transaction failed - account id " +
            std::to_string(id) + " does not exist");
        return false;
    }
	Account* acc = it->second.get();   
    pthread_mutex_unlock(&bank_mutex);

    if(!acc->getAccess(password)){ // wrong password
        logToFile("Error " + std::to_string(atm_id) +
                  ": Your transaction failed – password for account id " + std::to_string(id) + " is incorrect");
        return false;
    }
    int balance = acc->getBalance();
    logToFile(std::to_string(atm_id) + ": Account " + std::to_string(id)+ " balance is "+ 
    std::to_string(balance));
    return true;
}
bool Bank::closeAccount(int id, const std::string& password, int atm_id){
     pthread_mutex_lock(&bank_mutex);
     auto it = accounts.find(id); 
    if (it == accounts.end()) { // no account found
        pthread_mutex_unlock(&bank_mutex);
        logToFile("Error " + std::to_string(atm_id) + ": Your transaction failed - account id " +
            std::to_string(id) + " does not exist");
        return false;
    }
	Account* acc = it->second.get();   
    pthread_mutex_unlock(&bank_mutex);
    if(!acc->getAccess(password)){ // wrong password
        logToFile("Error " + std::to_string(atm_id) +
                  ": Your transaction failed – password for account id " + std::to_string(id) + " is incorrect");
        return false;
    }
    int balance = acc->getBalance();
    pthread_mutex_lock(&bank_mutex);
   // delete acc;
    accounts.erase(it);
    pthread_mutex_unlock(&bank_mutex);
    logToFile(std::to_string(atm_id) + ": Account " + std::to_string(id) +
                                 " is now closed. Balance was " + std::to_string(balance));
    return true;

}
bool Bank::transfer(int src_id, const std::string& password, int dst_id, int amount, int atm_id){
    if (src_id == dst_id) return false; 

    Account *src, *dst;

    pthread_mutex_lock(&bank_mutex);
    auto it_src = accounts.find(src_id);
    if (it_src == accounts.end()) { // src account not found
        pthread_mutex_unlock(&bank_mutex);
        logToFile("Error " + std::to_string(atm_id) + ": Your transaction failed - account id " +
            std::to_string(src_id) + " does not exist");
        return false;
    }
    auto it_dst = accounts.find(dst_id);
    if (it_dst == accounts.end()) { // dst account not found
        pthread_mutex_unlock(&bank_mutex);
        logToFile("Error " + std::to_string(atm_id) + ": Your transaction failed - account id " +
            std::to_string(dst_id) + " does not exist");
        return false;
    }
    src = it_src->second.get();
    dst = it_dst->second.get();
    pthread_mutex_unlock(&bank_mutex);


    if (!src->getAccess(password)) {
        logToFile("Error " + std::to_string(atm_id) + ": Your transaction failed – password for account id " + std::to_string(src_id) + " is incorrect");
        return false;
    }
    int src_newBalance = 0;
    if ((src_newBalance = src->withdraw(amount)) < 0) {
        logToFile("Error " + std::to_string(atm_id) + ": Your transaction failed – account id " + std::to_string(src_id) + " balance is lower than " + std::to_string(amount));
        return false;
    }

    int dst_newBalance = dst->deposit(amount);
    logToFile(std::to_string(atm_id) + ": Transfer " + std::to_string(amount) + " from account " +
              std::to_string(src_id) + " to account " + std::to_string(dst_id) +
              " new account balance is " + std::to_string(src_newBalance) +
              " new target account balance is " + std::to_string(dst_newBalance));
    return true;
}

bool Bank::closeATM(int src_id, int dst_id){
	if (dst_id <= 0 || dst_id == src_id) {
        logToFile("Error " + std::to_string(src_id) + ": Your transaction failed - ATM ID " + std::to_string(dst_id) + " does not exist");
        return false;
    }

    pthread_mutex_lock(&atm_mutex);
    for (auto atm : atms){
        if (atm->getAtmId() == dst_id){
            if(atm->getStopFlag()){ // already closed
                pthread_mutex_unlock(&atm_mutex);
                logToFile("Error " + std::to_string(src_id) + ": Your close operation failed - ATM ID " + std::to_string(dst_id) + " is already in a closed state");
                return false;
            }
            else{
                atm->markForClosure();
                pthread_mutex_unlock(&atm_mutex);
                logToFile("Bank: ATM " + std::to_string(src_id) + " closed " + std::to_string(dst_id) + " successfully");
                return true;
            }
            
        }
    }
    pthread_mutex_unlock(&atm_mutex);
    logToFile("Error " + std::to_string(src_id)+ ": Your transaction failed - ATM ID " + std::to_string(dst_id) + " does not exist");
    return false;

}
    
void Bank::logToFile(const std::string& msg) {
    pthread_mutex_lock(&log_mutex);
    logFile << msg << std::endl;
    pthread_mutex_unlock(&log_mutex);
}
