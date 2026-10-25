/*
 * Jimmy Paputto 2026
 */

#ifndef JIMMY_PAPUTTO_BLOCK_TERMINATION_SIGNALS_HPP_
#define JIMMY_PAPUTTO_BLOCK_TERMINATION_SIGNALS_HPP_

#include <pthread.h>
#include <signal.h>


namespace JimmyPaputto
{

// Blocks SIGINT and SIGTERM in the calling thread for the lifetime of the
// object. Threads started meanwhile inherit the mask, so the application's
// handlers never run in them and never interrupt their blocking I/O.
class BlockTerminationSignals
{
public:
    BlockTerminationSignals()
    {
        sigset_t signals;
        sigemptyset(&signals);
        sigaddset(&signals, SIGINT);
        sigaddset(&signals, SIGTERM);
        pthread_sigmask(SIG_BLOCK, &signals, &previous_);
    }

    ~BlockTerminationSignals()
    {
        pthread_sigmask(SIG_SETMASK, &previous_, nullptr);
    }

    BlockTerminationSignals(const BlockTerminationSignals&) = delete;
    BlockTerminationSignals& operator=(const BlockTerminationSignals&) = delete;

private:
    sigset_t previous_;
};

}  // JimmyPaputto

#endif  // JIMMY_PAPUTTO_BLOCK_TERMINATION_SIGNALS_HPP_
