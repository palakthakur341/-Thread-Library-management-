#include <iostream>
#include <queue>
#include <vector>
#include <functional>
#include <thread>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <condition_variable>

#define STACK_SIZE 1024 * 64 
#define MAX_THREADS 128     

enum ThreadState { READY, RUNNING, TERMINATED };

// Thread Control Block (TCB) structure
struct Thread {
    int id;
    ThreadState state;
    std::thread threadObj; 
    std::function<void()> task;
};

class ThreadLibrary {
private:
    int threadIdCounter;
    std::unordered_map<int, std::shared_ptr<Thread>> threads;
    std::queue<int> readyQueue;
    int currentThreadId;

    // Mutex for thread-safe operations
    std::mutex libMutex;
    static std::mutex printMutex;  

public:
    ThreadLibrary() : threadIdCounter(0), currentThreadId(-1) {}

    // Thread creation
    int createThread(std::function<void()> task) {
        std::lock_guard<std::mutex> lock(libMutex);

        if (threads.size() >= MAX_THREADS) {
            std::cerr << "Maximum thread limit reached!" << std::endl;
            return -1;
        }

        // Create a new thread
        std::shared_ptr<Thread> newThread = std::make_shared<Thread>();
        newThread->id = threadIdCounter++;
        newThread->state = READY;
        newThread->task = task;

        
        newThread->threadObj = std::thread([this, newThread]() {
            newThread->task();
            exitThread(); 
        });

        
        threads[newThread->id] = newThread;
        readyQueue.push(newThread->id);

        return newThread->id;
    }

    // Start scheduling
    void start() {
        while (!readyQueue.empty()) {
            currentThreadId = readyQueue.front();
            readyQueue.pop();

            auto thread = threads[currentThreadId];
            if (thread->state == TERMINATED) {
                continue;
            }

            thread->state = RUNNING;

            // Wait for the thread to finish its task
            if (thread->threadObj.joinable()) {
                thread->threadObj.join();
            }

            if (thread->state == TERMINATED) {
                threads.erase(currentThreadId);
            }
        }
    }

    // Exit the thread
    void exitThread() {
        std::lock_guard<std::mutex> lock(libMutex);

        auto thread = threads[currentThreadId];
        thread->state = TERMINATED;
    }

    // Mutex implementation
    class Mutex {
    private:
        bool locked;
        std::condition_variable cv;
        std::mutex mtx;

    public:
        Mutex() : locked(false) {}

        void lock() {
            std::unique_lock<std::mutex> lk(mtx);
            while (locked) {
                cv.wait(lk);
            }
            locked = true;
        }

        void unlock() {
            std::unique_lock<std::mutex> lk(mtx);
            locked = false;
            cv.notify_one();
        }
    };

    // Condition Variable implementation
    class ConditionVariable {
    private:
        std::condition_variable cv;

    public:
        void wait(std::unique_lock<std::mutex> &lk) {
            cv.wait(lk);
        }

        void notify_one() {
            cv.notify_one();
        }

        void notify_all() {
            cv.notify_all();
        }
    };

    
    static void synchronizedPrint(int id, int iteration) {
        std::lock_guard<std::mutex> lock(printMutex);
        std::cout << "Thread " << id << " is running iteration " << iteration << std::endl;
    }
};


std::mutex ThreadLibrary::printMutex; 

// Example usage
void threadTask(int id) {
    for (int i = 0; i < 5; ++i) {
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); 
        ThreadLibrary::synchronizedPrint(id, i + 1);  
    }
}

int main() {
    ThreadLibrary lib;

    // Create threads
    lib.createThread([]() { threadTask(1); });
    lib.createThread([]() { threadTask(2); });
    lib.createThread([]() { threadTask(3); });

    // Start the library
    lib.start();

    return 0;
}
