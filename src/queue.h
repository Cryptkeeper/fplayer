/// @file queue.h
/// @brief Playback queue interface.
#ifndef FPLAYER_QUEUE_H
#define FPLAYER_QUEUE_H

/// @struct qentry_s
/// @brief Queue entry structure that holds playback configuration data.
struct qentry_s {
    const char* seqfp;    ///< Sequence file path
    const char* audiofp;  ///< Audio override file path
    const char* cmapfp;   ///< Channel map file path
    unsigned int waitsec; ///< Playback start delay in seconds
};

/// @struct q_s
/// @brief Queue structure for managing playback requests.
struct q_s;

/// @brief Allocates and initializes a new queue instance.
/// @param q return pointer to the new queue instance
/// @return 0 on success, a negative error code on failure
int Q_init(struct q_s** q);

/// @brief Appends a new entry to the end of the queue.
/// @param q queue instance to add the entry to
/// @param ent entry to add to the queue, string pointers are copied but not the
/// strings themselves, the caller is responsible for managing the memory of the
/// strings to ensure they are valid for the lifetime of the queue
/// @return 0 on success, a negative error code on failure
int Q_append(struct q_s* q, struct qentry_s ent);

/// @brief Gets the next entry from the queue and removes it from the queue.
/// @param q queue instance to get the next entry from
/// @param ent return pointer to the next entry
/// @return 1 on success, 0 if the queue is empty
int Q_next(struct q_s* q, struct qentry_s* ent);

/// @brief Frees the memory allocated for the queue instance and all of its
/// remaining entries.
void Q_free(struct q_s* q);

#endif//FPLAYER_QUEUE_H
