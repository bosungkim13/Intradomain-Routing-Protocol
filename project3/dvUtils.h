#include <cstdint>
#include <vector>
#include <sharedUtils.h>
#include <unordered_map>
#include <limits>

// Distance Vector Packet Payload
struct DVPacketPayload {
    // TODO: isn
    seq_num sequenceNumber; // 16-bit sequence number
    std::vector<std::pair<router_id, time_stamp>> neighbors; // Vector of (neighborId, cost) pairs

    // Constructor to initialize sequence number
    DVPacketPayload(seq_num seqNum) : sequenceNumber(seqNum) {}

    // Add a neighbor entry with id and cost
    void addNeighbor(router_id neighborId, time_stamp cost) {
        neighbors.emplace_back(neighborId, cost);   // emplace_back() constructs element directly at the end of container to avoid temp variable
    }
};

// Distance Vector Route struct is used within the Distance Vector Routing Table
struct DVRoute {
    router_id nextHop;  // ID of the next hop router
    cost routeCost;     // Cost to reach the destination via this route

    // Constructor with default cost set to maximum value (representing infinity)
    DVRoute(router_id hop = 0, cost c = std::numeric_limits<cost>::max())
        : nextHop(hop), routeCost(c) {}
};

// Distance Vector Forwarding Table
struct DVForwardingTable {
    std::unordered_map<router_id, DVRoute> table;  // Mapping from destination router_id to Route

    // Add or update a route for a destination
    void updateRoute(router_id destination, router_id nextHop, cost routeCost) {
        table[destination] = DVRoute(nextHop, routeCost);
    }

    // Get the route for a given destination, if it exists
    DVRoute getRoute(router_id destination) const {
        auto it = table.find(destination);
        if (it != table.end()) {
            return it->second;
        }
        // Return a route with max cost if destination is not found (infinity equivalent)
        return DVRoute(0, std::numeric_limits<cost>::max());
    }

    // Remove a route for a given destination
    void removeRoute(router_id destination) {
        table.erase(destination);
    }
    
    // Check if a route exists for a destination
    bool hasRoute(router_id destination) const {
        return table.find(destination) != table.end();
    }
};

char* serializeDVPacket(DVPacketPayload serializeMe);

DVPacketPayload deserializeDVPayload(char* deserializeMe); // will fix later
