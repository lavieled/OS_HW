#include "account.h"

Account::Account(int id, const std::string& pw, int bal)
    : id(id), password(pw), balance(bal), r_counter(0) {
    pthread_mutex_init(&w_mutex, nullptr);
    pthread_mutex_init(&r_counter_mutex, nullptr);
}

Account::~Account() {
    pthread_mutex_destroy(&w_mutex);
    pthread_mutex_destroy(&r_counter_mutex);
}


// interface function:

bool Account::getAccess(const std::string& pw) const{
    std::string t = this->password;
    return (t == pw);
}
std::string Account::getPassword() const{
    return (this->password);
}
int Account::getId() const{
    return this->id;
}
int Account::getBalance(){
    pthread_mutex_lock(&r_counter_mutex);
    r_counter++;
    if(r_counter == 1)
        pthread_mutex_lock(&w_mutex);// reader lock write ability
    pthread_mutex_unlock(&r_counter_mutex);

    int t = this->balance;

    pthread_mutex_lock(&r_counter_mutex);
    r_counter--;
    if(r_counter == 0)
        pthread_mutex_unlock(&w_mutex);
    pthread_mutex_unlock(&r_counter_mutex);

    return (t);
}
int Account::deposit(int t){
    pthread_mutex_lock(&w_mutex);
    this->balance += t;
    int ret = this->balance;
    pthread_mutex_unlock(&w_mutex);
    return ret;
}

int Account::withdraw(int t){
    pthread_mutex_lock(&w_mutex);
    if ((this->balance - t) < 0){
        pthread_mutex_unlock(&w_mutex);
        return -1;
    }
    this->balance -= t;
    int ret = this->balance;
    pthread_mutex_unlock(&w_mutex);
    return ret;
}
