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

