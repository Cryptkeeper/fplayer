#ifndef PROTOWRITER_H
#define PROTOWRITER_H

struct LorBuffer;

/// @brief Allocates a new LOR message buffer. Caller is responsible for
/// freeing the buffer with `LB_free` when done.
/// @return pointer to the new buffer, or NULL if allocation failed
struct LorBuffer* LB_alloc(void);

/// @brief Frees a previously allocated LOR message buffer.
/// @param lb pointer to the buffer to free
void LB_free(struct LorBuffer* lb);

/// @brief Resets the write head of the LOR message buffer, enabling re-use of
/// a single allocation for multiple messages. Subsequent writes will overwrite
/// the previous message. This function does not clear the buffer.
/// @param lb pointer to the buffer to rewind
void LB_rewind(struct LorBuffer* lb);

#endif//PROTOWRITER_H
