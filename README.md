# Pizza Delivery System

## Description
This project implements a multithreaded **pizza delivery simulation** in C++, modeling real-world coordination between customers and drivers.  
It focuses on synchronization, concurrency control, and efficient resource sharing between multiple threads representing customers placing orders and drivers delivering them.

## Architecture
The system uses custom-built threading primitives:
- **Mutex:** Provides mutual exclusion to prevent race conditions.  
- **Condition Variable (CV):** Allows threads to wait and be signaled for specific events.  
- **Semaphore:** Controls access to limited shared resources, such as available delivery drivers.  

## Core Functionality
- Customers create orders and wait for drivers to be assigned.  
- Drivers handle one delivery at a time and notify customers upon arrival.  
- Proper synchronization ensures no deadlocks or data inconsistencies.  
- Simulates realistic delivery workflow timing, queueing, and order handling.
