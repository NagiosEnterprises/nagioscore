#include "signal_handler.h"


/**
 *  @function catch_signal     - signal action helper function.
 *  @param int signal          - OS SIG_NUM.
 *  @param void signal_handler - callback handler function on signal catch.
 *  @param int fillset         - boolean to set (fill) all signals or initialize empty.
 *  @param int bit_flags       - bit map to modify signal handling behavior.
 *
 *  @see https://linux.die.net/man/2/sigaction 
 */ 
extern int catch_signal(int signal, void (*signal_handler)(int), int fillset, int bit_flags) {
#ifdef HAVE_SIGACTION
    struct sigaction action;
    action.sa_sigaction = NULL;
    int catch = (1 == fillset) ? sigfillset(&action.sa_mask) : sigemptyset(&action.sa_mask);
    action.sa_handler = signal_handler;
    action.sa_flags = bit_flags;
    catch = sigaction(signal, &action, NULL);
#else
    int catch = signal(signal, signal_handler);
#endif
    return catch;
}

 
