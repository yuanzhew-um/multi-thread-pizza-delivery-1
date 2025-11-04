// pizza_delivery_system.cpp
#include "main_pizza.h"
#include <queue>

void delivery_init(uintptr_t);
void threads_initializer(unsigned int, unsigned int, std::vector<std::string>*);
void legal_matcher(uintptr_t);
void driver_thread(uintptr_t);
void customer_thread(uintptr_t);

struct MatchComparator {
    bool operator()(const std::tuple<int, int, int>& a, const std::tuple<int, int, int>& b) {
        return std::get<2>(a) > std::get<2>(b); // Min-heap based on distance
    }
};

void legal_matcher(uintptr_t) {
    while (true) {

        // Wait for ready customers
        no_driver_customers_mutex.lock();
        while (no_driver_customers.empty()) {
            no_driver_customers_cv.wait(no_driver_customers_mutex);
        }
        // Wait for ready drivers
        no_order_drivers_mutex.lock();
        while (no_order_drivers.empty()) {
            no_order_drivers_cv.wait(no_order_drivers_mutex);
        }
        // Priority queue to find closest pairs (driver_id, customer_id, distance)
        std::priority_queue<std::tuple<int, int, int>, std::vector<std::tuple<int, int, int>>, MatchComparator> potential_matches;

        // Build a priority queue of matches
        for (const auto& driver_entry : no_order_drivers) {
            auto driver = driver_entry.second;

            for (const auto& customer_entry : no_driver_customers) {
                auto customer = customer_entry.second;
                int distance = std::abs(static_cast<int>(driver->x - customer->x)) + std::abs(static_cast<int>(driver->y - customer->y));

                // Add pair to the priority queue
                potential_matches.emplace(driver->id, customer->id, distance);
            }
        }
        // Pick the top of the heap as the closest match
        auto [driver_id, customer_id, distance] = potential_matches.top();

        // Get matched driver and customer
        auto matched_driver = drivers[driver_id];
        auto matched_customer = customers[customer_id];

        // Lock the individual resources before updating statuses
        // std::cout << "INFO:::match try to get d " << driver_id << "& c << " << customer_id << "lock" << std::endl;
        matched_driver->mu_driver.lock();
        // std::cout << "INFO:::match get d lock success" << std::endl;
        matched_customer->mu_customer.lock();
        // std::cout << "INFO:::match get c lock success" << std::endl;

        // Update statuses
        matched_driver->stat = status_driver::MATCH;
        matched_driver->matched_customer_id = matched_customer->id;
        matched_customer->stat = status_customer::MATCH;
        matched_customer->matched_driver_id = matched_driver->id;

        // Tell the system match has been made
        match(customer_id, driver_id);

        // std::cout << "INFO:::match get d unlock" << std::endl;
        matched_driver->cv_driver.signal();
        matched_customer->cv_customer.signal();

        // std::cout << "INFO:::match get c unlock" << std::endl;
        matched_driver->mu_driver.unlock();
        matched_customer->mu_customer.unlock();
        
        // Remove matched entries from the waiting lists
        no_order_drivers.erase({matched_driver->id, matched_driver});
        no_driver_customers.erase({matched_customer->id, matched_customer});

        // Unlock large resources
        no_driver_customers_mutex.unlock();
        no_order_drivers_mutex.unlock();
        
    }
}

void delivery_init(uintptr_t msgs) {
    auto messages = reinterpret_cast<std::vector<std::string>*>(msgs);
    unsigned int num_drivers = std::stoi((*messages)[0]);
    unsigned int num_customers = std::stoi((*messages)[1]);
    threads_initializer(num_drivers, num_customers, messages);
}

void threads_initializer(unsigned int num_drivers, unsigned int num_customers, std::vector<std::string>* messages) {
    for (unsigned int i = 0; i < num_drivers; i++) {
        std::shared_ptr<Driver> d = std::make_shared<Driver>(i, 0, 0);
        drivers.push_back(d);
        thread dri(driver_thread, i);
    }
    for (unsigned int i = 0; i < num_customers; i++) {
        auto customer_file = (*messages)[i + 2];
        std::shared_ptr<Customer> c = std::make_shared<Customer>(i, customer_file);
        customers.push_back(c);
        thread cus(customer_thread, i);
    }
    thread match(legal_matcher, 0);
    // Do not delete messages here if it was not dynamically allocated
    // delete messages;
}

void customer_thread(uintptr_t c_id){
    auto cus = customers[c_id];
    auto file_name = cus->customer_file;
    std::ifstream ifstream(file_name);
    unsigned int x, y;
    //std::cout << "INFO:::mu_customer "<< c->id << " locked by customer thread id: " << c->id << std::endl;
    while (ifstream >> x >> y){
        cus->mu_customer.lock();
        cus->x = x;
        cus->y = y;
        no_driver_customers_mutex.lock();
        // std::cout << "INFO:::no_driver_customers_mutex locked by customer id: " << c->id << std::endl;
        no_driver_customers.insert({cus->id, cus});
        customer_ready(cus->id, {cus->x, cus->y});
        cus->stat = status_customer::READY_TO_MATCH;
        no_driver_customers_mutex.unlock();
        // std::cout << "INFO:::no_driver_customers_mutex unlocked by customer id: " << c->id << std::endl;
        no_driver_customers_cv.signal();
        // std::cout << "INFO:::mu_customer "<< c->id << " is trying to get by customer thread id: " << c->id << std::endl;
        while (cus->stat != status_customer::MATCH){
            cus->cv_customer.wait(cus->mu_customer);
        }
        // std::cout << "INFO:::mu_customer "<< c->id << " in customer's hand id: "<< c->id << std::endl;
        //after this, the no_driver_customer has erased the customer
        //and the no_order_drivers has erased the driver
        auto dri = drivers[cus->matched_driver_id];
        dri->mu_driver.lock();
        // std::cout << "INFO:::mu_driver " << d->id << " locked by customer thread id: " << c->id << std::endl;
        while (dri->stat != status_driver::ARRIVE){
            dri->cv_driver.wait(dri->mu_driver);
        }
        // std::cout << "INFO:::mu_driver " << d->id << " in customer's hand id: " << c->id << std::endl;
        pay(cus->id, dri->id);
        cus->stat = status_customer::PAY;
        dri->mu_driver.unlock();
        // std::cout << "INFO:::mu_driver " << d->id << " unlocked by customer thread id: " << c->id << std::endl;
        cus->cv_customer.signal();
        //after this step, driver should clean up the matched id
        while (cus->id == dri->matched_customer_id){
            cus->cv_customer.wait(cus->mu_customer);
        }
        // std::cout << "INFO:::mu_customer " << c->id << " in customer's hand id: " << c->id << std::endl;
        cus->matched_driver_id = -1;
        cus->mu_customer.unlock();
        // std::cout << "INFO:::mu_customer " << c->id << " unlocked by customer thread id: " << c->id << std::endl;
    }
}

void driver_thread(uintptr_t d_id){
    while (true){
        auto dri = drivers[d_id];
        dri->mu_driver.lock();
        // std::cout << "INFO:::no_order_drivers_mutex locked by driver id: " << d->id << std::endl;
        // std::cout << "INFO:::mu_driver " << d->id << " locked by driver id: " << d->id << std::endl;
        no_order_drivers_mutex.lock();
        no_order_drivers.insert({dri->id, dri});
        driver_ready(dri->id, {dri->x, dri->y});
        dri->stat = status_driver::READY_TO_MATCH;
        no_order_drivers_mutex.unlock();
        // std::cout << "INFO:::no_order_drivers_mutex unlocked by driver id: " << d->id << std::endl;
        no_order_drivers_cv.signal();
        while (dri->stat != status_driver::MATCH){
            dri->cv_driver.wait(dri->mu_driver);
        }
        // std::cout << "INFO:::mu_driver "<< d->id << " locked in driver's hand id: " << d->id << std::endl;
        //after this, he no_order_drivers has erased the driver
        //and the no_driver_customers has erased the customer
        auto cus = customers[dri->matched_customer_id];
        location_t c_loc = {cus->x, cus->y};
        location_t d_loc = {dri->x, dri->y};
        drive(dri->id, d_loc, c_loc);
        dri->x = cus->x;
        dri->y = cus->y;
        dri->stat = status_driver::ARRIVE;

        dri->mu_driver.unlock();
        // std::cout << "INFO:::mu_driver "<< d->id << " unlocked by driver id: " << d->id << std::endl;
        dri->cv_driver.signal();
    
        // std::cout << "INFO:::mu_customer "<< c->id << " locked by driver thread id: " << d->id << std::endl;
        cus->mu_customer.lock();
        while (cus->stat != status_customer::PAY){
            cus->cv_customer.wait(cus->mu_customer);
        }
        // std::cout << "INFO:::mu_customer "<< c->id << " in driver's hand id: "<< d->id << std::endl;
        dri->matched_customer_id = -1;
        cus->mu_customer.unlock();
        // std::cout << "INFO:::mu_customer "<< c->id << " unlocked by driver thread id: " << d->id << std::endl;
        cus->cv_customer.signal();

    }
}

int main(int argc, char* argv[]) {
    if (argc <= 2) {
        std::cerr << "Usage: " << argv[0] << " <num_drivers> <customer_in_files>" << std::endl;
        return 1;
    }

    //argv[1] is number of drivers
    //startig from argv[2] is the customer files
    std::string num_drivers = argv[1];
    std::string num_customers = std::to_string(argc - 2);
    std::vector<std::string> msgs;
    msgs.push_back(num_drivers);
    msgs.push_back(num_customers);
    for (int i = 2; i < argc; i++){
        //push back the customer files
        msgs.push_back(argv[i]);
    }

    // Boot the thread library and start the parent function
    cpu::boot(delivery_init, reinterpret_cast<uintptr_t>(&msgs), 1);
}
