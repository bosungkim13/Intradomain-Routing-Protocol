#ifndef DVUTILS_H
#define DVUTILS_H
#include <cstdint>
#include <vector>
#include "sharedUtils.h"
#include <unordered_map>
#include <unordered_set>
#include <limits>
#include <stdlib.h>
#include <cassert>
#include <arpa/inet.h>

// Distance Vector Route struct is used within the Distance Vector Routing Table
struct ForwardingEntry
{
    router_id nextHop; // ID of the next hop router
    cost routeCost;    // Cost to reach the destination via this route
    time_stamp lastUpdate; // Time of last update

    // Constructor with default cost set to maximum value (representing infinity)
    ForwardingEntry(router_id hop = 0, cost c = USHRT_MAX, time_stamp t = 0);
};

// Distance Vector Forwarding Table
struct DVForwardingTable
{
    unordered_map<router_id, ForwardingEntry> table; // Mapping from destination router_id to Route (which contains nextHop and routeCost)
    Node * context;

    DVForwardingTable(); // Default constructor is unused, but necessary for compilation
    
    DVForwardingTable(Node * n); // need to pass in Node pointer as context for getting the time.

    // Add or update a route for a destination
    void updateRoute(router_id destination, router_id nextHop, cost routeCost, bool verb = false);

    // Get the route for a given destination, if it exists
    ForwardingEntry getRoute(router_id destination) const;

    // Remove a route for a given destination
    void removeRoute(router_id destination);

    unordered_set<router_id> removeRoutesWithNextHop(router_id nextHop);

    // Check if a route exists for a destination
    bool hasRoute(router_id destination) const;

    void printTable();
};

DVForwardingTable deserializeDVPayload(Packet packet, Node * n);

struct RouteInfo {
    router_id nextHop;
    cost routeCost;

    RouteInfo(router_id nextHop, cost routeCost);
};

struct DVBigTable {
    unordered_map<router_id, unordered_map<router_id, cost>> table;

    // Add or update a route for a destination
    void updateRoute(router_id destination, router_id nextHop, cost routeCost, bool verb = false);

    // Gets the best route for a given destination, but returns a route with cost USHRT_MAX if no route exists
    RouteInfo getBestRoute(router_id destination) const;

    // Remove a route for a given destination
    void removeRoute(router_id destination, router_id nextHop);

    // Remove routes that depend on a given next hop
    void removeRoutesWithNextHop(router_id nextHop);

    // Check if a route exists for a destination
    bool hasRoute(router_id destination) const;

    // Print the table
    void printTable() const;
};


#endif
