#ifndef WORKERPOOL_H
#define WORKERPOOL_H

#include <vector>
#include <memory>

namespace Concurrent {

struct ITask
{
    virtual ~ITask() = default;
    virtual void Perform() = 0;
};

struct IWorker
{
    virtual ~IWorker() = default;
    virtual bool AssignTask(std::unique_ptr<ITask> &&task) = 0;
};

class WorkerPool
{
public:
    explicit WorkerPool(unsigned pool_size);

    virtual ~WorkerPool();
    virtual IWorker *SubmitTask(std::unique_ptr<ITask> &&task) = 0;

    void Start();
    void Quit();
    void Wait();
protected:
    struct Worker;
    std::vector<std::unique_ptr<Worker>> workers;
};

class RoundRobinWorkerPool : public WorkerPool
{
public:
    explicit RoundRobinWorkerPool(unsigned pool_size);

    IWorker *SubmitTask(std::unique_ptr<ITask> &&task) override;
private:
    size_t next_worker;
};

}

#endif
