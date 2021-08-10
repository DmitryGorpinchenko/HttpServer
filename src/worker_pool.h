#ifndef WORKERPOOL_H
#define WORKERPOOL_H

#include <vector>
#include <memory>

namespace Concurrent {

struct Task {
    virtual ~Task() = default;

    virtual void Perform() = 0;
};

class WorkerPool
{
public:
    explicit WorkerPool(unsigned pool_size);
    virtual ~WorkerPool();

    void Start();
    void Quit();
    void Wait();

    virtual void SubmitTask(std::unique_ptr<Task> &&task) = 0;
protected:
    struct Worker;
    std::vector<std::unique_ptr<Worker>> workers;
};

class RoundRobinWorkerPool : public WorkerPool
{
public:
    explicit RoundRobinWorkerPool(unsigned pool_size);

    void SubmitTask(std::unique_ptr<Task> &&task) override;
private:
    size_t next_worker;
};

}

#endif
