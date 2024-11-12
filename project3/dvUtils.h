#ifndef DVUTILS_H
#define DVUTILS_H
#include <cstdint>
#include <vector>
#include "sharedUtils.h"
#include <unordered_map>
#include <limits>
#include <stdlib.h>
#include <cassert>
#include <arpa/inet.h>
#include <Node.h>

// Distance Vector Route struct is used within the Distance Vector Routing Table
struct DVRoute
{
    router_id nextHop; // ID of the next hop router
    cost routeCost;    // Cost to reach the destination via this route
    time_stamp lastUpdate; // Time of last update

    // Constructor with default cost set to maximum value (representing infinity)
    DVRoute(router_id hop = 0, cost c = USHRT_MAX, time_stamp t = 0)
        : nextHop(hop), routeCost(c), lastUpdate(t) {}
};

// Distance Vector Forwarding Table
struct DVForwardingTable
{
    unordered_map<router_id, DVRoute> table; // Mapping from destination router_id to Route (which contains nextHop and routeCost)
    Node * context;

    DVForwardingTable(Node * n) : context(n) {} // need to pass in Node pointer as context for getting the time.

    // Add or update a route for a destination
    void updateRoute(router_id destination, router_id nextHop, cost routeCost)
    {
        table[destination] = DVRoute(nextHop, routeCost, context->time());
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
        return DVRoute(0, USHRT_MAX, context->time()); // return the current time for now?
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
};

DVForwardingTable deserializeDVPayload(Packet packet, Node * n) 
{
    unsigned int numEntries = (packet.header.size - HEADER_SIZE) / (2 * sizeof(router_id) + sizeof(cost));

    assert((packet.header.size - HEADER_SIZE) % (2 * sizeof(router_id) + sizeof(cost)) == 0); // TEMPORARY CODE, DELETE LATER

    DVForwardingTable table(n);
    size_t offset = 0;

    // Payload alternates between destination ID, nextHop ID, and routeCost
    for (unsigned int i = 0; i < numEntries; i++) {
        router_id destID, nextHop;
        cost routeCost;

        
        destID = ntohs(*reinterpret_cast<unsigned short*>(packet.payload[offset]));
        offset += sizeof(router_id);

        nextHop = ntohs(*reinterpret_cast<unsigned short*>(packet.payload[offset]));
        offset += sizeof(router_id);

        routeCost = ntohs(*reinterpret_cast<unsigned short*>(packet.payload[offset]));
        offset += sizeof(cost);

        table.updateRoute(destID, nextHop, routeCost); // updateRoute should already take care of the timestamp stuff
    }

    return table;
}

#endif
