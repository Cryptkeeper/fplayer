#ifndef PROTOWRITER_H
#define PROTOWRITER_H

struct LorBuffer;

/// @brief Allocates a new LOR message buffer. Caller is responsible for
/// freeing the allocation when it is no longer needed.
/// @return pointer to the new buffer, or NULL if allocation failed
struct LorBuffer* LB_alloc(void);

/// @brief Resets the write head of the LOR message buffer, enabling re-use of
/// a single allocation for multiple messages. Subsequent writes will overwrite
/// the previous message. This function does not clear the buffer.
/// @param lb pointer to the buffer to rewind
void LB_rewind(struct LorBuffer* lb);

#endif//PROTOWRITER_H
