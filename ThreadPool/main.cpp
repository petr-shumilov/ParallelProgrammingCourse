#include <iostream>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <mutex>
#include <queue>
#include <functional>
#include <any>
#include <atomic>


enum TaskStatus { TASK_NEW, TASK_RUNNING, TASK_FINISHED };
int TaskIDCounter = 1;

template<typename R, typename ...A>
using TaskFunction = std::function<R(A...)>;


class ITask {
public:
    virtual ~ITask() = default;
    virtual void SetArg(std::any) = 0;
    virtual void Run() = 0;
    virtual int GetID() = 0;
    virtual bool IsCompleted() = 0;
    virtual void PushContinuationsIntoQueue(std::queue<ITask*>&) = 0;
};

template <typename R, typename ...A>
class Task : public virtual ITask {
public:
    Task(TaskFunction<R, A...> _taskFunction, A... _args) : taskFunction(_taskFunction), args(_args...), taskStatus(TASK_NEW), id(TaskIDCounter++) {}

    R GetResult() {
        while (this->taskStatus != TASK_FINISHED) {
            std::this_thread::yield();
        }
        return this->result;
    }

    void SetArg(std::any arg) override {
        using argType = typename std::tuple_element<0, std::tuple<A...>>::type;
        this->args = std::make_tuple(std::any_cast<argType>(arg));
    }

    void Run() override {
        this->taskStatus = TASK_RUNNING;
        this->result = std::apply([this](auto &&... args) -> R { return this->taskFunction(args...); }, this->args);
        this->taskStatus = TASK_FINISHED;
    }

    int GetID() override {
        return this->id;
    }

    bool IsCompleted() override {
        return this->taskStatus == TASK_FINISHED;
    }

    template <typename NR>
    Task<NR, R>* ContinueWith(TaskFunction<NR, R> continueFunc) {
        auto newTask = new Task<NR, R>(continueFunc, {});
        {
            std::unique_lock<std::mutex> locker(this->continuationsQueueLock);
            this->continuationsQueue.push(newTask);
        }
        return newTask;
    }

    void PushContinuationsIntoQueue(std::queue<ITask*>& q) override {
        std::unique_lock<std::mutex> contQueueLocker(this->continuationsQueueLock);
        while (!this->continuationsQueue.empty()) {
            auto contTask = this->continuationsQueue.front();
            this->continuationsQueue.pop();
            contTask->SetArg(this->GetResult());
            q.push(contTask);
        }
    }

private:
    int id;
    TaskStatus taskStatus;
    TaskFunction<R, A...> taskFunction;
    std::tuple<A...> args;
    R result;
    std::mutex continuationsQueueLock;
    std::queue<ITask*> continuationsQueue;
};



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



int foo(int a) {
    std::this_thread::sleep_for(std::chrono::seconds(a));
    return 1;
}
int goo(int a) {
    std::this_thread::sleep_for(std::chrono::seconds(a));
    return 20;
}
int bar(int a) {
    std::this_thread::sleep_for(std::chrono::seconds(a));
    return 30;
}

std::string boo(int a) {
    std::this_thread::sleep_for(std::chrono::seconds(a));
    return "string";
}

int main()
{

    Task<int, int>* task1 = new Task<int, int>(foo, 5);
    auto task2 = new Task<int, int>(goo, 10);
    auto  task3 = new Task<int, int>(foo, 20);
    auto  task4 = new Task<int, int>(bar, 3);
    auto  task5 = new Task<int, int>(goo, 4);
    auto  task6 = new Task<int, int>(bar, 2);

    task1->ContinueWith<std::string>(boo);
    auto res = task1->ContinueWith<std::string>(boo);


    ThreadPool threadPool(6);

    std::cout<< "Number of workers: " << threadPool.NumOfRunningThreads() << "\n";



    std::this_thread::sleep_for(std::chrono::seconds(1));
    threadPool.Enqueue(task1);
    threadPool.Enqueue(task2);
    threadPool.Enqueue(task3);
    threadPool.Enqueue(task4);
    threadPool.Enqueue(task5);
    threadPool.Enqueue(task6);


    std::this_thread::sleep_for(std::chrono::seconds(10));

    task1->ContinueWith<std::string>(boo);
    threadPool.Enqueue(task1);


    threadPool.Shutdown();

}