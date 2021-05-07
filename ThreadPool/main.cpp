#include <iostream>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <mutex>
#include <queue>
#include <functional>

std::condition_variable WorkerCondVar;
std::mutex ThreadPoolQueueLock;
std::mutex PrintLock;

bool ThreadPoolRunning = true;
using TaskFunction = std::function<int()>;
int TaskIDCounter = 1;

enum TaskStatus { TASK_NEW, TASK_RUNNING, TASK_FINISHED };

class Task {
public:
    Task(TaskFunction _taskFunction) : taskFunction(_taskFunction), taskStatus(TASK_NEW), id(TaskIDCounter++) {};

    int GetResult() {
        while (this->taskStatus != TASK_FINISHED);
        return this->result;
    }

    void Run() {
        this->taskStatus = TASK_RUNNING;
        this->result = this->taskFunction();
        this->taskStatus = TASK_FINISHED;
    }

    int GetID() {
        return this->id;
    }

    bool IsComplete() {
        return this->taskStatus == TASK_FINISHED;
    }

private:
    int id;
    TaskStatus taskStatus;
    TaskFunction taskFunction;
    int result;
};

std::queue<Task*> ThreadPoolQueue;


void Worker() {
    while (ThreadPoolRunning) {
        {
            std::unique_lock<std::mutex> locker(PrintLock);
            std::cout << "[" << std::this_thread::get_id() << "]: IDLE... \n";
        }
        std::unique_lock<std::mutex> locker(ThreadPoolQueueLock);
        WorkerCondVar.wait(locker, [&](){ return !ThreadPoolQueue.empty(); });


        auto task = ThreadPoolQueue.front();
        ThreadPoolQueue.pop();

        locker.unlock();

        {
            std::unique_lock<std::mutex> locker(PrintLock);
            std::cout << "[" << std::this_thread::get_id() << "]: Captured task#" << task->GetID() << "\n";
        }


        task->Run();


        {
            std::unique_lock<std::mutex> locker(PrintLock);
            std::cout << "[" << std::this_thread::get_id() << "]: Finished task#" << task->GetID() << std::endl;
        }
    }
}


int foo() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return 10;
}
int goo() {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    return 20;
}
int bar() {
    std::this_thread::sleep_for(std::chrono::seconds(10));
    return 30;
}

int main()
{

    auto task1 = new Task(foo);
    auto task2 = new Task(goo);
    auto task3 = new Task(foo);
    auto task4 = new Task(bar);
    auto task5 = new Task(goo);
    auto task6 = new Task(bar);


    std::thread t1(Worker), t2(Worker), t3(Worker), t4(Worker);



    std::this_thread::sleep_for(std::chrono::seconds(1));
    ThreadPoolQueue.push( task1);
    ThreadPoolQueue.push( task2);
    ThreadPoolQueue.push( task3);
    ThreadPoolQueue.push( task4);
    ThreadPoolQueue.push( task5);
    ThreadPoolQueue.push( task6);


    WorkerCondVar.notify_all();



    t1.join();
    t2.join();
    t3.join();
    t4.join();
}