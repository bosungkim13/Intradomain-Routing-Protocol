#include <cstdint>
#include <vector>
#include <sharedUtils.h>

// Distance Vector Packet Payload
struct DVPacketPayload {
    seq_num sequenceNumber; // 16-bit sequence number
    std::vector<std::pair<router_id, time_stamp>> neighbors; // Vector of (neighborId, cost) pairs

    // Constructor to initialize sequence number
    DVPacketPayload(seq_num seqNum) : sequenceNumber(seqNum) {}

    // Add a neighbor entry with id and cost
    void addNeighbor(router_id neighborId, time_stamp cost) {
        neighbors.emplace_back(neighborId, cost);   // emplace_back() constructs element directly at the end of container to avoid temp variable
    }
};