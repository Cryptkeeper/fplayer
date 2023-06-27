#ifndef FPLAYER_LOR_H
#define FPLAYER_LOR_H

#include <stdbool.h>
#include <stdint.h>

/*
 *  lor.h provides a single-threaded, fixed-size append-only unsafe buffer mechanism
 * for encoding LOR-protocol packets.
 *
 *  Caller is given a pointer via `bufhead()` where they are responsible for writing
 * data. Function does not check for remaining buffer space, caller should take
 * care to not overrun the backing <2k buffer. This is an unsafe pattern, but it
 * is intended only for coupling for liblightorama encoding calls which operate
 * output <32 byte packet sizes.
 *
 *  If a caller knows the write size ahead of time, they may call `bufadv(size)` to
 * ensure the available capacity (via assert which is included in release builds).
 * This provides a (fatal) bounds checking method.
 *
 *  For liblightorama encoding calls, `bufadv` is likely called last once the function
 * has returned the encoded length of data written to `bufhead()`.
 *
 *  `bufadv` increments the write head index, and encoding functions must call
 * `bufhead()` to receive a new write pointer.
 *
 *  After each "writing block" (whatever the caller defines as a decent chunk of
 * data having been written), `bufflush` should be called with `force=true` and
 * a "transfer" function pointer responsible for receiving the buffered data output.
 *
 *  The transfer function has access to the buffered data via its parameters only
 * for the scope of the function. Once the transfer callback returns, the buffer
 * is reset via `bufreset()` and the write head zeroed. Callers should copy
 * the data to their own allocated buffer if they wish to preserve it.
 *
 *  Using `force=false` with `bufflush` allows the caller to suggest moments where
 * the buffer may opt to flush its accumulated buffer to the transfer function.
 * `force=true` ensures the buffer is flushed.
 *
 *  I shortened my normal prefix convention (`lorBuffer$`) to `buf$` since the
 * functions are heavily repeated in most usage patterns. Plus, it makes these
 * functions feel as dangerous as they are :)
 *
 */

uint8_t *bufhead(void);

void bufadv(int size);

typedef void (*buf_transfer_t)(const uint8_t *b, int size);

void bufflush(bool force, buf_transfer_t transfer);

#endif//FPLAYER_LOR_H
