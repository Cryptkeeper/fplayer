#ifndef FPLAYER_FC_H
#define FPLAYER_FC_H

#include <stdbool.h>
#include <stdint.h>

/// @brief File controller that wraps a stdlib file pointer and provides
/// additional functionality for reading files, including thread-safe access.
struct FC;

/// @brief Opens a file controller instance for the given file path.
/// @param fp file path to open
/// @return a file controller instance, or NULL if an error occurred
struct FC* FC_open(const char* fp);

/// @brief Closes the given file controller instance and frees its resources.
/// @param fc target file controller instance
void FC_close(struct FC* fc);

/// @brief Reads the given number of bytes from the file controller starting at
/// the given offset.
/// @param fc target file controller instance
/// @param offset offset in bytes from the start of the file
/// @param size number of bytes to read
/// @param b buffer to read into
/// @return the number of bytes read, otherwise 0 or a value less than `size`
/// if an error occurred
uint32_t FC_read(struct FC* fc, uint32_t offset, uint32_t size, uint8_t* b);

/// @brief Reads up to the given number of bytes from the file controller
/// starting at the given offset, up to the given maximum count of bytes or
/// until the end of the file is reached.
/// @param fc target file controller instance
/// @param offset offset in bytes from the start of the file
/// @param size number of bytes to read
/// @param maxCount maximum number of bytes to read
/// @param b buffer to read into
/// @return the number of bytes read, otherwise 0 or a value less than `size`
/// if an error occurred
uint32_t FC_readto(struct FC* fc,
                   uint32_t offset,
                   uint32_t size,
                   uint32_t maxCount,
                   uint8_t* b);

/// @brief Returns the size of the file backing the given file controller. This
/// performs a seek to the end of the file to determine its size and then seeks
/// back to the beginning.
/// @param fc target file controller instance
/// @return the size of the file in bytes, or 0 if an error occurred
uint32_t FC_filesize(struct FC* fc);

/// @brief Returns the file path used to open the given file controller.
/// @param fc target file controller instance
/// @return the file path
const char* FC_filepath(const struct FC* fc);

#endif//FPLAYER_FC_H
