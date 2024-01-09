// packet.h
#ifndef PACKET_H
#define PACKET_H

#define MAX_TEXT_LENGTH 1024

typedef struct packet {
    char text[MAX_TEXT_LENGTH];
    char sNummer[7];
} Packet;

#endif // PACKET_H