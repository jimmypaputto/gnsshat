/*
 * Jimmy Paputto 2025
 */

#ifndef JIMMY_PAPUTTO_NOTIFIER_HPP_
#define JIMMY_PAPUTTO_NOTIFIER_HPP_

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <stop_token>


namespace JimmyPaputto
{

class Notifier
{
public:
    void notify()
    {
        std::lock_guard lock(mtx);
        ready = true;
        ++generation;
        cv.notify_all();
    }

    void wait()
    {
        std::unique_lock lock(mtx);
        cv.wait(lock, [this]{ return ready; });
        ready = false;
    }

    bool wait(std::stop_token stoken)
    {
        std::unique_lock lock(mtx);
        if (!cv.wait(lock, stoken, [this]{ return ready; }))
            return false;
        ready = false;
        return true;
    }

    // Side subscriber: keeps its own cursor instead of consuming `ready`, so
    // it neither steals nor misses the notifications seen by wait().
    bool waitForNewGeneration(uint64_t& cursor, std::stop_token stoken)
    {
        std::unique_lock lock(mtx);
        if (!cv.wait(lock, stoken, [this, &cursor]{ return generation != cursor; }))
            return false;
        cursor = generation;
        return true;
    }

private:
    std::mutex mtx;
    std::condition_variable_any cv;
    bool ready = false;
    uint64_t generation = 0;
};

}  // JimmyPaputto

#endif  // JIMMY_PAPUTTO_NOTIFIER_HPP_
