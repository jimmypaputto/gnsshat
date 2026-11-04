/*
 * Jimmy Paputto 2026
 */

#ifndef JIMMY_PAPUTTO_THREADING_SIGNALS_HPP_
#define JIMMY_PAPUTTO_THREADING_SIGNALS_HPP_

#include <pthread.h>
#include <signal.h>

#include <thread>
#include <utility>


namespace JimmyPaputto
{

// Blocks every asynchronous signal in the calling thread for the lifetime of
// the object, then restores the thread's previous mask. Threads started
// meanwhile inherit the full mask, so signals reach only the application's
// threads and never interrupt the library's blocking I/O. The synchronous
// fault signals stay unblocked: they always go to the faulting thread, and a
// blocked one would bypass the application's crash handler.
class BlockAllSignals
{
public:
    BlockAllSignals()
    {
        sigset_t signals;
        sigfillset(&signals);
        sigdelset(&signals, SIGBUS);
        sigdelset(&signals, SIGFPE);
        sigdelset(&signals, SIGILL);
        sigdelset(&signals, SIGSEGV);
        pthread_sigmask(SIG_BLOCK, &signals, &previous_);
    }

    ~BlockAllSignals()
    {
        pthread_sigmask(SIG_SETMASK, &previous_, nullptr);
    }

    BlockAllSignals(const BlockAllSignals&) = delete;
    BlockAllSignals& operator=(const BlockAllSignals&) = delete;

private:
    sigset_t previous_;
};

// Every thread of the library is started through this, so none of them can
// take a signal meant for the application. The caller's mask is unchanged
// once it returns.
template<class Thread = std::jthread, class... Args>
Thread createSignalFreeThread(Args&&... args)
{
    const BlockAllSignals blocked;
    return Thread(std::forward<Args>(args)...);
}

}  // JimmyPaputto

#endif  // JIMMY_PAPUTTO_THREADING_SIGNALS_HPP_
