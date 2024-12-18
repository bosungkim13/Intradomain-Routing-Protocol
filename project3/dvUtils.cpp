#include "dvUtils.h"
#include "VariadicTable.h"
#include <unordered_set>

ForwardingEntry::ForwardingEntry(router_id hop, cost c, time_stamp t)
    : nextHop(hop), routeCost(c), lastUpdate(t) {};

RouteInfo::RouteInfo(router_id nextHop, cost routeCost)
    : nextHop(nextHop), routeCost(routeCost) {};

DVForwardingTable::DVForwardingTable(Node *n)
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
    if (verbose) cout << "Removing route to " << destination << endl;
    table.erase(destination);
}

// modded version of remove route, when a link from A to B dies, it removes
// routes that take B as the next hop
unordered_set<router_id> DVForwardingTable::removeRoutesWithNextHop(router_id nextHop)
{
    unordered_set<router_id> removeSet;
    for (auto row : table)
    {
        router_id destID = row.first;
        ForwardingEntry route = row.second;
        if (route.nextHop == nextHop)
        {
            removeSet.insert(destID);
        }
    }
    for (router_id destID : removeSet)
    {
        table.erase(destID);
        if (verbose)
            if (verbose) cout << "Removing route to " << destID << " as it was routed through the dead link to " << nextHop << endl;
    }
    return removeSet;
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

DVForwardingTable deserializeDVPayload(Packet packet, Node *n)
{
    unsigned int numEntries = (packet.header.size - HEADER_SIZE) / (2 * sizeof(router_id) + sizeof(cost));

    assert((packet.header.size - HEADER_SIZE) % (2 * sizeof(router_id) + sizeof(cost)) == 0); // TEMPORARY CODE, DELETE LATER

    DVForwardingTable table(n);
    size_t offset = 0;

    // Payload alternates between destination ID, nextHop ID, and routeCost
    for (unsigned int i = 0; i < numEntries; i++)
    {
        router_id destID, nextHop;
        cost routeCost;

        destID = ntohs(*reinterpret_cast<unsigned short *>(&packet.payload[offset]));
        offset += sizeof(router_id);

        nextHop = ntohs(*reinterpret_cast<unsigned short *>(&packet.payload[offset]));
        offset += sizeof(router_id);

        routeCost = ntohs(*reinterpret_cast<unsigned short *>(&packet.payload[offset]));
        offset += sizeof(cost);

        table.updateRoute(destID, nextHop, routeCost); // updateRoute should already take care of the timestamp stuff
    }

    return table;
}

//////////////////
// DV Big Table

// Add or update a route for a destination
void DVBigTable::updateRoute(router_id destination, router_id nextHop, cost routeCost, bool verb)
{
    table[destination][nextHop] = routeCost;
}

RouteInfo DVBigTable::getBestRoute(router_id destination) const
{
    auto destIt = table.find(destination);
    if (destIt == table.end() || destIt->second.empty())
    {
        // If the destination doesn't exist or has no valid routes, return a default DVRoute
        return RouteInfo(0, USHRT_MAX-1); // Infinite cost or nextHop == 0 indicates no valid route (NOTE: assuming that 0 as node is invalid)
    }

    if (verbose) cout << "destination " << destination << " was found in the big table" << endl;
    const auto &nextHops = destIt->second;
    router_id bestNextHop = 0;
    cost lowestCost = USHRT_MAX;
    time_stamp bestLastUpdated = 0;

    for (const auto &pair : nextHops)
    {
        const router_id nextHop = pair.first;
        const cost routeCost = pair.second;

        if (routeCost < lowestCost)
        {
            bestNextHop = nextHop;
            lowestCost = routeCost;
        }
    }

    return RouteInfo(bestNextHop, lowestCost);
}

// Remove a route for a given destination
void DVBigTable::removeRoute(router_id destination, router_id nextHop)
{
    auto destIt = table.find(destination);
    if (destIt != table.end())
    {
        auto &nextHops = destIt->second;
        auto nextHopIt = nextHops.find(nextHop);
        if (nextHopIt != nextHops.end())
        {
            nextHops.erase(nextHopIt); // Erase the next hop
            if (verbose) std::cout << "Route to destination " << destination
                      << " via next hop " << nextHop << " removed.\n";
            if (nextHops.empty())
            {
                table.erase(destIt); // Remove destination if no next hops remain
                if (verbose) std::cout << "Destination " << destination << " removed due to no remaining routes.\n";
            }
            return; // Since nextHop is unique, we can exit after removal
        }
        else
        {
            if (verbose) std::cout << "No route via next hop " << nextHop
                      << " for destination " << destination << ".\n";
        }
    }
    else
    {
        if (verbose) std::cout << "Destination " << destination << " not found in the table.\n";
    }
}

// Remove routes that depend on this nextHop
void DVBigTable::removeRoutesWithNextHop(router_id nextHop)
{
    // Iterate through every possible destination
    for (auto destIt = table.begin(); destIt != table.end();)
    {
        auto &nextHops = destIt->second;
        auto nextHopIt = nextHops.find(nextHop);
        if (nextHopIt != nextHops.end())
        {
            // Erase the route for this next hop
            if (verbose) std::cout << "Removing route to destination " << destIt->first
                      << " via next hop " << nextHop << ".\n";
            nextHops.erase(nextHopIt);

            // If no more next hops exist for this destination, erase the destination
            if (nextHops.empty())
            {
                if (verbose) std::cout << "Removing destination " << destIt->first
                          << " as it has no remaining routes.\n";
                destIt = table.erase(destIt); // Erase destination and move to next
            }
            else
            {
                ++destIt; // If next hops still exist, just move to next destination
            }
        }
        else
        {
            ++destIt; // Move to the next destination if nextHop isn't found
        }
    }
}

// Check if a route exists for a destination
bool DVBigTable::hasRoute(router_id destination) const
{
    auto it = table.find(destination);
    return it != table.end() && !it->second.empty();
}

// Print the routing table
void DVBigTable::printTable() const
{
    VariadicTable<router_id, router_id, cost> vt({"Destination", "Next Hop", "Route Cost"});
    for (const auto &row : table)
    {
        for (const auto &hop : row.second)
        {
            vt.addRow(row.first, hop.first, hop.second);
        }
    }
    vt.print(cout);
}