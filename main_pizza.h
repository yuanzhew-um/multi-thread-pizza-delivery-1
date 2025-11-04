#include <vector>
#include "cpu.h"
#include "thread.h"
#include "mutex.h"
#include "cv.h"
#include "pizza.h"
#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_set>

mutex mtx;

enum class status_customer
{   
    INIT,
    READY_TO_MATCH,
    PAY,
    MATCH
};
enum class status_driver
{
    INIT,
    READY_TO_MATCH,
    ARRIVE,
    MATCH
};

mutex no_order_drivers_mutex = mutex(), no_driver_customers_mutex = mutex();
cv no_order_drivers_cv = cv(), no_driver_customers_cv = cv();

struct Driver
{
    unsigned int id;   // Driver ID
    unsigned int x, y; // Driver's current location
    //std::shared_ptr<Customer> matched_customer;
    unsigned int matched_customer_id;
    bool driver_aval;                         // Is the driver free for a delivery
    status_driver stat = status_driver::INIT; // Starting off with wait condition, waiting for wake up
    cv cv_driver;                             // CV specifically for driver, avoid random signal
    mutex mu_driver;
    Driver(unsigned int id, unsigned int x, unsigned int y)
        : id(id), x(x), y(y), driver_aval(false){
            cv_driver = cv();
            mu_driver = mutex();
        }
};

struct Customer
{
    unsigned int id;   // Customer ID
    unsigned int x, y; // Customer's requested location
    //std::shared_ptr<Driver> matched_driver;
    unsigned int matched_driver_id;                   // Has the customer placed order yet
    status_customer stat = status_customer::INIT; // Starting off with wait condition, waiting for wake up
    cv cv_customer;                               // CV specifically for customer, avoid random signal
    mutex mu_customer;
    std::string customer_file;
    bool customer_aval;        
    Customer(unsigned int id, std::string file)
        : id(id), customer_file(file), customer_aval(false){
            cv_customer = cv();
            mu_customer = mutex();
        }
};

std::vector<std::shared_ptr<Driver>> drivers;
std::vector<std::shared_ptr<Customer>> customers;

struct PairHash {
    template <typename T1, typename T2>
    std::size_t operator()(const std::pair<T1, T2>& p) const {
        auto hash1 = std::hash<T1>{}(p.first);
        auto hash2 = std::hash<T2>{}(p.second);
        return hash1 ^ hash2;
    }
};

std::unordered_set<std::pair<int, std::shared_ptr<Driver>>, PairHash> no_order_drivers;      // Ready driver to be find legal match
std::unordered_set<std::pair<int, std::shared_ptr<Customer>>, PairHash> no_driver_customers; // Ready customer to be find legal match