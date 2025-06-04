#include "bank.h"
#include "atm.h"  
#include <vector>
#include <string>
#include <fstream>
#include <iostream>

std::ofstream logFile("log.txt");

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Bank error: illegal arguments" << std::endl;
        return 1;
    }

    /*init the bank
	constructor also opens the bank's comission account
	*/
    Bank* bank = new Bank();

    //open the banks threads
    bank->startBankThreads();

    
    //init all atm args and start proccesing commands
    for (int i = 1; i < argc; ++i) {
        std::string atmFile = argv[i];
        ATM* atm = new ATM(i, atmFile, bank);
        pthread_mutex_lock(&bank->atm_mutex);//lock atms
        bank->atms.push_back(atm);
        pthread_mutex_unlock(&bank->atm_mutex);//unlock atms
        atm->start(); // atm thread
    }

    //wait for all atms to finish running
    for (auto atm : bank->atms) {
        atm->join();
    }

	//for (auto atm : bank->atms) {
	    //delete atm;
	//}//free memory
	

    
    delete bank;
    logFile.close();

    return 0;
}
