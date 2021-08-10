#include "worker_pool.h"
#include "message_queue.h"

#include <thread>

namespace Concurrent {

struct WorkerPool::Worker
{
    MessageQueue<std::unique_ptr<Task>> task_queue;
    std::thread thr;

    void Start();
    void Quit();
    void Wait();

    bool AssignTask(std::unique_ptr<Task> &&task);

    void Run();
};

void WorkerPool::Worker::Start()
{
    if (!thr.joinable()) {
        thr = std::thread(&Worker::Run, this);
    }
}

void WorkerPool::Worker::Quit()
{
    task_queue.StopReceiving();
}

void WorkerPool::Worker::Wait()
{
    if (thr.joinable()) {
        thr.join();
    }
}

bool WorkerPool::Worker::AssignTask(std::unique_ptr<Task> &&task)
{
    return task_queue.Send(std::move(task));
}

void WorkerPool::Worker::Run()
{
    while (true) {
        try {
            auto task = task_queue.Receive();
            task->Perform();
        } catch (MessageQueue<std::unique_ptr<Task>>::ReceivingStopped &) {
            break;
        }
    }
}

//

WorkerPool::WorkerPool(unsigned pool_size)
{
    for (unsigned i = 0; i < pool_size; ++i) {
        workers.push_back(std::unique_ptr<Worker>(new Worker));
    }
}

WorkerPool::~WorkerPool()
{
    Quit();
    Wait();
}

void WorkerPool::Start()
{
    for (auto &w : workers) {
        w->Start();
    }
}

void WorkerPool::Quit()
{
    for (auto &w : workers) {
        w->Quit();
    }
}

void WorkerPool::Wait()
{
    for (auto &w : workers) {
        w->Wait();
    }
}

//

RoundRobinWorkerPool::RoundRobinWorkerPool(unsigned pool_size)
    : WorkerPool(pool_size)
    , next_worker(0)
{
}

void RoundRobinWorkerPool::SubmitTask(std::unique_ptr<Task> &&task)
{
    workers[next_worker]->AssignTask(std::move(task));
    next_worker = (next_worker + 1) % workers.size();
}

}

