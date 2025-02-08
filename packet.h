#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>
#include "params.h"

#pragma pack(1)
typedef struct {
    uint32_t index;
    char sender[NAMESIZE];
    char message[MAXDATASIZE];
} Packet;
#pragma pack()

#endif // !PACKET_H
#define PACKET_H

