#ifndef MESSAGEQUEUE_H
#define MESSAGEQUEUE_H

#include <deque>
#include <limits>
#include <mutex>
#include <condition_variable>

namespace Concurrent {

template <typename T>
class MessageQueue
{
public:
    struct ReceivingStopped
    {
    };

    explicit MessageQueue(size_t _max_size = std::numeric_limits<size_t>::max());

    size_t Size() const;
    size_t MaxSize() const;

    bool Send(T &&msg);
    T Receive();

    void StopReceiving();
private:
    std::condition_variable cv;
    mutable std::mutex mtx;
    bool stop_receiving;
    
    std::deque<T> queue;
    size_t max_size;
};

template <typename T>
MessageQueue<T>::MessageQueue(size_t _max_size)
    : stop_receiving(false)
    , max_size(_max_size)
{
}

template <typename T>
size_t MessageQueue<T>::Size() const
{
    std::lock_guard<std::mutex> lock(mtx);
    return queue.size();
}

template <typename T>
size_t MessageQueue<T>::MaxSize() const
{
    return max_size;
}

template <typename T>
bool MessageQueue<T>::Send(T &&msg)
{
    bool res = false;
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (queue.size() < max_size) {
            queue.push_back(std::move(msg));
            res = true;
        }
    }
    if (res) {
        cv.notify_one();
    }
    return res;
}

template <typename T>
T MessageQueue<T>::Receive()
{
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [this]() { return (!queue.empty()) || stop_receiving; });

    if (stop_receiving) {
        throw ReceivingStopped();
    }

    T msg = std::move(queue.front());
    queue.pop_front();
    return msg;
}

template <typename T>
void MessageQueue<T>::StopReceiving()
{
    {
        std::lock_guard<std::mutex> lock(mtx);
        stop_receiving = true;
    }
    cv.notify_one();
}

}

#endif
