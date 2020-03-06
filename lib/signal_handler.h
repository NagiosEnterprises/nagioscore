#ifndef LIBNAGIOS_signal_handler_h__
#define LIBNAGIOS_signal_handler_h__

/**
 * @file signal_handler.h
 * @brief Signal handling helper function(s) 
 * 
 * This is a short addendum to handle all signals in a single helper function.
 * @{
 */

extern int catch_signal(int signal, void (*signal_handler)(int), int fillset, int bit_flags); 

/** @} */
#endif /* LIBNAGIOS_bitmap_h__ */ 

