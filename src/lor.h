#ifndef FPLAYER_LOR_H
#define FPLAYER_LOR_H

// guidance size for a stack-based Light-O-Rama packet encoding buffer
// no LOR packet emitted by this code should exceed half this size
#define LOR_PACKET_BUFFER 32

#define lorInitBuffer(var_name) uint8_t var_name[LOR_PACKET_BUFFER] = {0}

#endif//FPLAYER_LOR_H
