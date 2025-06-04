#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <string>
#include <pthread.h>

class Account {
public:
    const int id;
    const std::string password;
    int balance;

    Account(int id, const std::string& pw, int bal);
    ~Account();

    int getId() const; 
    bool getAccess(const std::string& pw) const;
    std::string getPassword() const;
    int getBalance();
    int deposit(int amount);
    int withdraw(int amount);

private:
    pthread_mutex_t w_mutex;
    pthread_mutex_t r_counter_mutex;
    int r_counter;
};

#endif
