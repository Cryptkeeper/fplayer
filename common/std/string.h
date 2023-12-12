#ifndef FPLAYER_STRING_H
#define FPLAYER_STRING_H

// dsprintf is a dynamic string formatter that returns a pointer to a newly
// allocated string. The caller is responsible for freeing the returned string.
char *dsprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif//FPLAYER_STRING_H
