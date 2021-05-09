#include <iostream>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <mutex>
#include <queue>
#include <functional>
#include <any>
#include <atomic>
#include <assert.h>
#include "ThreadPool.h"

int FibSum(int a){
    int sum = 0, p1 = 1, p2 = 2;
    for (auto _ = a; _--;) {
        int i = p1 + p2;
        p1 = p2;
        p2 = i;
        sum += i;
    }
    return sum;
}

std::string IntToStr(int a) {
    return std::to_string(a);
}

long UpToNSum(long n){
    long sum = 0;
    for (int j = 0; j < n; ++j) {
        sum += j;
    }
    return sum;
}

std::string AddSurroundBraces(std::string str) {
    return "[" + str + "]";
}

void checkThreadPoolWorkersAmountTest() {
    const int workersNum = 10;
    ThreadPool threadPool(workersNum);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    assert(threadPool.NumOfRunningThreads() == workersNum);

    threadPool.Shutdown();
}

void singleTaskTest() {
    const int workersNum = 10;
    const int fibN = 10;
    const int trueSum = 605;
    ThreadPool threadPool(workersNum);

    auto t = new Task<int, int>(FibSum, fibN);
    threadPool.Enqueue(t);

    assert(t->GetResult() == trueSum);

    threadPool.Shutdown();
}

void multipleTaskTest() {
    const int workersNum = 10;
    const int taskNum = 100;
    const int trueSum = 161700;
    ThreadPool threadPool(workersNum);

    std::vector<Task<int, int>*> tasks;

    for (int i = 0; i < taskNum; ++i) {
        auto t = new Task<int, int>(UpToNSum, i);
        tasks.emplace_back(t);
        threadPool.Enqueue(t);
    }

    long sum = 0;
    for(auto const& t: tasks) {
        sum += t->GetResult();
    }
    assert(sum == trueSum);

    threadPool.Shutdown();
}

void singleContinueWithTest() {
    const int workersNum = 10;
    const int fibN = 10;
    const int trueSum = 605;
    const std::string trueStr = "605";

    ThreadPool threadPool(workersNum);

    auto task = new Task<long, long>(FibSum, fibN);
    auto contTask = task->ContinueWith<std::string>(IntToStr);

    threadPool.Enqueue(task);

    assert(task->GetResult() == trueSum);
    assert(contTask->GetResult() == trueStr);

    threadPool.Shutdown();
}

void multipleContinueWithTest() {
    const int workersNum = 10;
    const int fibN = 10;
    const int trueSum = 605;
    const std::string trueStr = "605";
    const std::string trueTrueStr = "[605]";

    ThreadPool threadPool(workersNum);

    auto task = new Task<long, long>(FibSum, fibN);
    auto contTask = task->ContinueWith<std::string>(IntToStr);
    auto contContTask = contTask->ContinueWith<std::string>(AddSurroundBraces);

    threadPool.Enqueue(task);

    assert(task->GetResult() == trueSum);
    assert(contTask->GetResult() == trueStr);
    assert(contContTask->GetResult() == trueTrueStr);

    threadPool.Shutdown();
}

int main() {
    checkThreadPoolWorkersAmountTest();
    singleTaskTest();
    multipleTaskTest();
    singleContinueWithTest();
    multipleContinueWithTest();
}