#ifndef SHAREDUTILS_H
#define SHAREDUTILS_H
#include <global.h>
#include <Node.h>

#define MAX_PACKET_SIZE 65535 // 2^16 - 1
#define HEADER_SIZE sizeof(PacketHeader)
#define MAX_PAYLOAD_SIZE MAX_PACKET_SIZE - HEADER_SIZE

// Type aliases for convenience
typedef unsigned short port_num;
typedef unsigned short router_id;
typedef unsigned short cost;
typedef unsigned short time_stamp;
typedef unsigned int seq_num;

// Define Neighbor struct
struct Neighbor {
    port_num port;
    cost timeCost;
    Neighbor() : port(0), timeCost(0) {}
    Neighbor(port_num p, cost c) : port(p), timeCost(c) {}
};

// Define PortSatusEntry struct
struct PortStatusEntry {
    time_stamp lastUpdate;
    cost timeCost;
    router_id destRouterID;
    bool isUp;
    PortStatusEntry() : lastUpdate(0), timeCost(0), destRouterID(0), isUp(false) {}
};

typedef std::unordered_map<router_id, int>& seqNum_ref;
typedef std::unordered_map<router_id, Neighbor>& adjacencyList_ref; // Adjust type as needed
typedef std::unordered_map<port_num, PortStatusEntry>& portStatus_ref; // Adjust type as needed
typedef std::unordered_map<router_id, router_id>& forwardingTable_ref;    // Adjust type as needed

// Define PacketHeader struct
struct PacketHeader {
    unsigned char packetType; // 8-bit packet type
    unsigned char reserved; // 8-bit reserved section
    unsigned short size; // 16-bit size of the entire packet in bytes
    unsigned short sourceID; // 16-bit source router ID
    unsigned short destID; // 16-bit destination router ID
    PacketHeader(unsigned char type, unsigned short size, unsigned short srcID, unsigned short dstID)
    : packetType(type), size(size), sourceID(srcID), destID(dstID), reserved(0) {} // Initialize members
    PacketHeader() : packetType(0), reserved(0), size(0), sourceID(0), destID(0) {} // Default constructor
};

// Define PingPongPacket struct
struct Packet {
    PacketHeader header; // Packet header
    char payload[MAX_PAYLOAD_SIZE];
    Packet() {} // Default constructor
};

enum eAlarmType {
    PingPongAlarm,
    UpdateAlarm,
    FreshnessAlarm,
};


void* serializePacket(Packet serializeMe);
Packet deserializePacket(void* deserializeMe);

# endif