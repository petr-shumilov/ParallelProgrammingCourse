#include <iostream>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <mutex>
#include <queue>
#include <functional>
#include <any>
#include <atomic>
#include "Task.h"

class ThreadPool {
public:
    ThreadPool(int threadsNum) : isRunning(true) {
        for (auto _ = threadsNum; _--;) {
            this->workerThreads.emplace_back(std::thread(&ThreadPool::workerRun, this));
        }
    }

    void Enqueue(ITask* task) {
        if (this->isRunning.load()) {
            std::unique_lock <std::mutex> queueLocker(this->queueLock);
            this->queue.push(task);
            this->workerCondVar.notify_all();
        }
    }

    void Join() {
        for (std::thread& t : this->workerThreads) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    void Shutdown() {
        this->isRunning.store(false);
        this->workerCondVar.notify_all();
        this->Join();
    }

    int NumOfRunningThreads() {
        int num = 0;
        for (std::thread& t : this->workerThreads) {
            num += t.joinable() ? 1 : 0;
        }
        return num;
    }

    ~ThreadPool() {
        this->Join();
    }

private:
    void workerRun() {
        while (this->isRunning.load() || !this->queueIsEmpty()) {

            this->log("IDLE...");

            std::unique_lock<std::mutex> queueLocker(this->queueLock);
            this->workerCondVar.wait(queueLocker, [&](){ return !this->queue.empty() || !this->isRunning.load(); });

            if (!this->isRunning.load() && this->queue.empty()) {
                queueLocker.unlock();
                break;
            }

            auto task = this->queue.front();
            this->queue.pop();

            queueLocker.unlock();

            this->log("Captured task #" + std::to_string(task->GetID()));

            if (task->IsCompleted()) {
                queueLocker.lock();
                task->PushContinuationsIntoQueue(this->queue);
                queueLocker.unlock();

                this->log("Already finished task #" + std::to_string(task->GetID()) + ", performing continuations...");
            } else {

                try {
                    task->Run();
                } catch (const std::exception& e) {
                    std::throw_with_nested(e);
                }

                queueLocker.lock();
                task->PushContinuationsIntoQueue(this->queue);
                queueLocker.unlock();

                this->log("Finished task #" + std::to_string(task->GetID()));
            }
        }
    }

    bool queueIsEmpty() {
        std::unique_lock<std::mutex> queueLocker(this->queueLock);
        return this->queue.empty();
    }

    void log(std::string text) {
        std::unique_lock<std::mutex> locker(this->logLock);
        std::cout << "[" << std::this_thread::get_id() << "]: " << text << std::endl;
    }


private:
    std::vector<std::thread> workerThreads;
    std::queue<ITask*> queue;

    std::condition_variable workerCondVar;
    std::mutex queueLock;
    std::mutex logLock;

    std::atomic<bool> isRunning;
};