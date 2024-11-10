#ifndef DVUTILS_H
#define DVUTILS_H
#include <cstdint>
#include <vector>
#include "sharedUtils.h"
#include <unordered_map>
#include <limits>
#include <stdlib.h>

// // Distance Vector Packet Payload

// struct DVPacketPayload
// {
//     seq_num sequenceNumber; // 16-bit sequence number

//     // I think it makes more sense to store the routing table, because the neighbors are already in there.
//     std::vector<std::pair<router_id, time_stamp>> neighbors; // Vector of (neighborId, cost) pairs

//     // Constructor to initialize sequence number
//     // I don't think DV requires a sequence number.
//     DVPacketPayload(seq_num seqNum) : sequenceNumber(seqNum) {}

//     // Add a neighbor entry with id and cost
//     void addNeighbor(router_id neighborId, time_stamp cost)
//     {
//         neighbors.emplace_back(neighborId, cost); // emplace_back() constructs element directly at the end of container to avoid temp variable
//     }
// };

// Distance Vector Route struct is used within the Distance Vector Routing Table
struct DVRoute
{
    router_id nextHop; // ID of the next hop router
    cost routeCost;    // Cost to reach the destination via this route

    // Constructor with default cost set to maximum value (representing infinity)
    DVRoute(router_id hop = 0, cost c = std::numeric_limits<cost>::max())
        : nextHop(hop), routeCost(c) {}
};

// Distance Vector Forwarding Table
struct DVForwardingTable
{
    std::unordered_map<router_id, DVRoute> table; // Mapping from destination router_id to Route (which contains nextHop and routeCost)

    // Add or update a route for a destination
    void updateRoute(router_id destination, router_id nextHop, cost routeCost)
    {
        table[destination] = DVRoute(nextHop, routeCost);
    }

    // Get the route for a given destination, if it exists
    DVRoute getRoute(router_id destination) const
    {
        auto it = table.find(destination);
        if (it != table.end())
        {
            return it->second;
        }
        // Return a route with max cost if destination is not found (infinity equivalent)
        return DVRoute(0, std::numeric_limits<cost>::max());
    }

    // Remove a route for a given destination
    void removeRoute(router_id destination)
    {
        table.erase(destination);
    }

    // Check if a route exists for a destination
    bool hasRoute(router_id destination) const
    {
        return table.find(destination) != table.end();
    }

    // Payload of a DV Packet == Distance Vector Forwarding Table
    char *serializeDVPayload()
    {
        char* buffer = (char *) malloc(table.size() * (2 * sizeof(router_id) + sizeof(cost)));
        size_t offset = 0;

        unsigned long size = table.size();
        memcpy(buffer + offset, &size, sizeof(size_t));
        offset += sizeof(size_t);

        for (const auto& nbr : table) {
            router_id destID = nbr.first;
            router_id nextHop = nbr.second.nextHop;
            cost routeCost = nbr.second.routeCost;

            memcpy(buffer + offset, &destID, sizeof(router_id));
            offset += sizeof(router_id);
            memcpy(buffer + offset, &nextHop, sizeof(router_id));
            offset += sizeof(router_id);
            memcpy(buffer + offset, &routeCost, sizeof(cost));
            offset += sizeof(cost);
        }

        return buffer;
    }   


    static DVForwardingTable deserializeDVPayload(char *dvPayload) 
    {
        DVForwardingTable table;
        size_t offset = 0;

        // Read table size
        size_t tableSize;
        std::memcpy(&tableSize, dvPayload + offset, sizeof(tableSize));
        offset += sizeof(tableSize);

        // Deserialize each entry
        for (uint32_t i = 0; i < tableSize; ++i)
        {
            router_id destination, nextHop;
            cost routeCost;

            // Read destination router_id
            std::memcpy(&destination, dvPayload + offset, sizeof(router_id));
            offset += sizeof(router_id);

            // Read nextHop and routeCost for DVRoute
            std::memcpy(&nextHop, dvPayload + offset, sizeof(router_id));
            offset += sizeof(router_id);
            std::memcpy(&routeCost, dvPayload + offset, sizeof(cost));
            offset += sizeof(cost);

            // Update the table with the deserialized route
            table.updateRoute(destination, nextHop, routeCost);
        }
        return table;
    }
};
#endif