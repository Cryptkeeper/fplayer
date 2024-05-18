#ifndef FPLAYER_SL_H
#define FPLAYER_SL_H

typedef char* slist_t;

/// @brief `sladd()` duplicates a string and appends it to a string list. The
/// string list is dynamically resized as needed and returned via the `sl` param.
/// Caller is responsible for freeing the list using `slfree`.
/// @param sl The string list to append to, and the return value of the resized list.
/// @param str The string to duplicate and append.
/// @return 0 if successful and `sl` is set to the new string list,
/// otherwise -1 is returned and `errno` is set.
int sladd(slist_t** sl, const char* str);

/// @brief `slfree()` frees a string list and all its elements.
void slfree(slist_t* sl);

#endif//FPLAYER_SL_H
