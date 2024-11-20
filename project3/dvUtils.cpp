#include "dvUtils.h"
#include "VariadicTable.h"
#include <unordered_set>

ForwardingEntry::ForwardingEntry(router_id hop, cost c, time_stamp t)
    : nextHop(hop), routeCost(c), lastUpdate(t) {};

DVForwardingTable::DVForwardingTable(Node * n)
    : context(n) {};

DVForwardingTable::DVForwardingTable()
    : context(nullptr) {};

void DVForwardingTable::updateRoute(router_id destination, router_id nextHop, cost routeCost, bool verb)
{
    // print update statement for debug
    table[destination] = ForwardingEntry(nextHop, routeCost, context->time());
}

ForwardingEntry DVForwardingTable::getRoute(router_id destination) const
{
    auto it = table.find(destination);
    if (it != table.end())
    {
        return it->second;
    }
    // Return a route with max cost if destination is not found (infinity equivalent)
    return ForwardingEntry(0, USHRT_MAX, context->time()); // return the current time for now?
}

void DVForwardingTable::removeRoute(router_id destination)
{
    // print for debug
    cout << "Removing route to " << destination << endl;
    table.erase(destination);
}

// modded version of remove route, when a link from A to B dies, it removes
// routes that take B as the next hop 
void DVForwardingTable::removeRouteModded(router_id destination) {
    unordered_set<router_id> removeSet;
    for (auto row : table) {
        router_id destID = row.first;
        ForwardingEntry route = row.second;
        if (route.nextHop == destination) {
            removeSet.insert(destID);
        }
    }
    for (router_id destID : removeSet) {
        table.erase(destID);
        cout << "Removing route to " << destID << " as it was routed through the dead link to " << destination << endl;
    }
}


bool DVForwardingTable::hasRoute(router_id destination) const
{
    return table.find(destination) != table.end();
}

void DVForwardingTable::printTable()
{
    VariadicTable<router_id, router_id, cost, time_stamp> vt({"Destination", "Next Hop", "Route Cost", "Last Update"});
    for (auto row : table)
    {
        vt.addRow(row.first, row.second.nextHop, row.second.routeCost, row.second.lastUpdate);
    }
    vt.print(cout);
}

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

        
        destID = ntohs(*reinterpret_cast<unsigned short*>(&packet.payload[offset]));
        offset += sizeof(router_id);

        nextHop = ntohs(*reinterpret_cast<unsigned short*>(&packet.payload[offset]));
        offset += sizeof(router_id);

        routeCost = ntohs(*reinterpret_cast<unsigned short*>(&packet.payload[offset]));
        offset += sizeof(cost);

        table.updateRoute(destID, nextHop, routeCost); // updateRoute should already take care of the timestamp stuff
    }
    
    return table;
}



////////////////// 
// DV Big Table

struct DVBigTable {
    unordered_map<router_id, unordered_map<router_id, cost>> table;

    // Add or update a route for a destination
    void updateRoute(router_id destination, router_id nextHop, cost routeCost, bool verb = false) {
        if (verb) {
            cout << "Updating route to " << destination << " via " << nextHop
                 << " with cost " << routeCost << endl;
        }
        table[destination][nextHop] = routeCost;
    }

    ForwardingEntry DVBigTable::getBestRoute(router_id destination) const {
        auto destIt = table.find(destination);
        if (destIt == table.end() || destIt->second.empty()) {
            // If the destination doesn't exist or has no valid routes, return a default DVRoute
            return ForwardingEntry(0, USHRT_MAX, 0); // Infinite cost indicates no valid route
        }

        const auto& nextHops = destIt->second;
        router_id bestNextHop = 0;
        cost lowestCost = USHRT_MAX;
        time_stamp bestLastUpdated = 0;

        for (const auto& pair : nextHops) {
            const router_id nextHop = pair.first;
            const DVRouteInfo& routeInfo = pair.second;

            if (routeInfo.routeCost < lowestCost) {
                bestNextHop = nextHop;
                lowestCost = routeInfo.routeCost;
                bestLastUpdated = routeInfo.lastUpdated;
            }
        }

        return ForwardingEntry(bestNextHop, lowestCost, bestLastUpdated);
    }

    // Remove a route for a given destination
    void removeRoute(router_id destination) {
        cout << "Removing all routes to " << destination << endl;
        table.erase(destination);
    }

    // Remove routes that rely on a specific next hop
    void removeRouteModded(router_id destination) {
        unordered_set<router_id> toRemove;
        for (auto &entry : table) {
            auto &nextHops = entry.second;
            if (nextHops.erase(destination) > 0) {
                cout << "Removing route to " << entry.first
                     << " as it was routed through the dead link to " << destination << endl;
            }
            if (nextHops.empty()) {
                toRemove.insert(entry.first);
            }
        }
        for (router_id dest : toRemove) {
            table.erase(dest);
        }
    }

    // Check if a route exists for a destination
    bool hasRoute(router_id destination) const {
        auto it = table.find(destination);
        return it != table.end() && !it->second.empty();
    }

    // Print the routing table
    void printTable() {
        VariadicTable<router_id, router_id, cost> vt({"Destination", "Next Hop", "Route Cost"});
        for (const auto &row : table) {
            for (const auto &hop : row.second) {
                vt.addRow(row.first, hop.first, hop.second);
            }
        }
        vt.print(cout);
    }
};