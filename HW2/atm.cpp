#include "atm.h"
#include <fstream>
#include <sstream>
#include <unistd.h> // usleep
#include <iostream>
#include "bank.h"

ATM::ATM(int id, const std::string& filename, Bank* bank)
    : atm_id(id), filename(filename), bank(bank), stopFlag(false), markedForClosure(false) {}

ATM::~ATM() {}

void ATM::start() {
    pthread_create(&thread, nullptr, runHelper, this);
}

void ATM::join() {
    pthread_join(thread, nullptr);
}

void* ATM::runHelper(void* arg) {
    ATM* atm = static_cast<ATM*>(arg);
    atm->run();
    return nullptr;
}

void ATM::setStopFlag() { // bank approves req to shutdown atm
    stopFlag = true;
}

bool ATM::getStopFlag() { // bank use getter to see if stopped
    return stopFlag;
}

    int  ATM::getAtmId(){
        return this->atm_id;
    }
//atm run func parses and proccess commands and send to bank for exec
void ATM::run() {
    std::ifstream infile(filename);
    std::string line;

    //while (!stopFlag && std::getline(infile, line)) {
    while (std::getline(infile, line)) {
        if (stopFlag) break;

        usleep(100 * 1000); // 100ms
	std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "O") {
            int id, amount;
            std::string pw;
            if (!(iss >> id >> pw >> amount) || !iss.eof()) {
                bank->logToFile("Error " + std::to_string(atm_id) + ": Malformed O command");
                continue;
            }
            bank->openAccount(id, pw, amount, atm_id);
        }
	else if (cmd == "D") {
            int id, amount;
            std::string pw;
            if (!(iss >> id >> pw >> amount) || !iss.eof()) {
                bank->logToFile("Error " + std::to_string(atm_id) + ": Malformed D command");
                continue;
            }
            bank->deposit(id, pw, amount, atm_id);
        }
	else if (cmd == "W") {
            int id, amount;
            std::string pw;
            if (!(iss >> id >> pw >> amount) || !iss.eof()) {
                bank->logToFile("Error " + std::to_string(atm_id) + ": Malformed W command");
                continue;
            }
            bank->withdraw(id, pw, amount, atm_id);
        }
	else if (cmd == "B") {
            int id;
            std::string pw;
            if (!(iss >> id >> pw) || !iss.eof()) {
                bank->logToFile("Error " + std::to_string(atm_id) + ": Malformed B command");
                continue;
            }
            bank->balanceInquiry(id, pw, atm_id);
        }
	else if (cmd == "Q") {
            int id;
            std::string pw;
            if (!(iss >> id >> pw) || !iss.eof()) {
                bank->logToFile("Error " + std::to_string(atm_id) + ": Malformed Q command");
                continue;
            }
            bank->closeAccount(id, pw, atm_id);
        }
	else if (cmd == "T") {
            int src, dst, amount;
            std::string pw;
            if (!(iss >> src >> pw >> dst >> amount) || !iss.eof()) {
                bank->logToFile("Error " + std::to_string(atm_id) + ": Malformed T command");
                continue;
            }
            bank->transfer(src, pw, dst, amount, atm_id);
        }
	else if (cmd == "C") {
            int dst_id;
            if (!(iss >> dst_id) || !iss.eof()) {
                bank->logToFile("Error " + std::to_string(atm_id) + ": Malformed C command");
                continue;
            }
            bank->closeATM(atm_id, dst_id);
        }
        else {
            bank->logToFile("Error " + std::to_string(atm_id) + ": Unknown command " + cmd);
        }

        usleep(1000 * 1000); // 1 second between commands
    }
}
