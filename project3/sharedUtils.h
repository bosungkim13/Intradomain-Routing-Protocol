#ifndef SHAREDUTILS_H
#define SHAREDUTILS_H
#include "global.h"
#include "Node.h"
#include <cstring>
#include <arpa/inet.h>

#define MAX_PACKET_SIZE 65535 // 2^16 - 1
#define HEADER_SIZE sizeof(PacketHeader)
#define MAX_PAYLOAD_SIZE MAX_PACKET_SIZE - HEADER_SIZE

#define verbose 0

// Type aliases for convenience
typedef unsigned short port_num;
typedef unsigned short router_id;
typedef unsigned short cost;
typedef unsigned long time_stamp;
typedef unsigned int seq_num;

// Define Neighbor struct
struct Neighbor {
    port_num port;
    cost timeCost;
    Neighbor() : port(0), timeCost(0) {}
    Neighbor(port_num p, cost c) : port(p), timeCost(c) {}
};

// Define PortStatusEntry struct
struct PortStatusEntry {
    time_stamp lastUpdate;
    cost timeCost;
    router_id destRouterID;
    bool isUp;
    PortStatusEntry() : lastUpdate(0), timeCost(0), destRouterID(0), isUp(false) {}
};

typedef std::unordered_map<router_id, int>& seqNum_ref;
typedef std::unordered_map<router_id, Neighbor>& adjacencyList_ref;
typedef std::unordered_map<port_num, PortStatusEntry>& portStatus_ref;
typedef std::unordered_map<router_id, router_id>& forwardingTable_ref;

typedef std::unordered_map<router_id, Neighbor>* adjacencyList_ptr; 
typedef std::unordered_map<port_num, PortStatusEntry>* portStatus_ptr;
typedef std::unordered_map<router_id, router_id>* forwardingTable_ptr;

// Define PacketHeader struct
#pragma pack(push, 1)
struct PacketHeader {
    unsigned char packetType; // 8-bit packet type
    unsigned char reserved; // 8-bit reserved section
    unsigned short size; // 16-bit size of the entire packet in bytes
    unsigned short sourceID; // 16-bit source router ID
    unsigned short destID; // 16-bit destination router ID
    PacketHeader(unsigned char type, unsigned short size, unsigned short srcID, unsigned short dstID)
    : packetType(type), reserved(0), size(size), sourceID(srcID), destID(dstID) {} // Initialize members
    PacketHeader() : packetType(0), reserved(0), size(0), sourceID(0), destID(0) {} // Default constructor
};

// Define PingPongPacket struct
struct Packet {
    PacketHeader header; // Packet header
    char payload[MAX_PAYLOAD_SIZE];
    Packet() {} // Default constructor
};
#pragma pack(pop)

enum eAlarmType {
    PingPongAlarm,
    UpdateAlarm,
    FreshnessAlarm,
};


void* serializePacket(Packet serializeMe);
Packet deserializePacket(void* deserializeMe);

# endif