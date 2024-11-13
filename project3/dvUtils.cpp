#include "dvUtils.h"
#include "VariadicTable.h"

DVRoute::DVRoute(router_id hop, cost c, time_stamp t)
    : nextHop(hop), routeCost(c), lastUpdate(t) {};

DVForwardingTable::DVForwardingTable(Node * n)
    : context(n) {};

DVForwardingTable::DVForwardingTable()
    : context(nullptr) {};

void DVForwardingTable::updateRoute(router_id destination, router_id nextHop, cost routeCost)
{
    // print update statement for debug
    cout << "Updating route to " << destination << " via " << nextHop << " with cost " << routeCost << endl;
    table[destination] = DVRoute(nextHop, routeCost, context->time());
}

DVRoute DVForwardingTable::getRoute(router_id destination) const
{
    auto it = table.find(destination);
    if (it != table.end())
    {
        return it->second;
    }
    // Return a route with max cost if destination is not found (infinity equivalent)
    return DVRoute(0, USHRT_MAX, context->time()); // return the current time for now?
}

void DVForwardingTable::removeRoute(router_id destination)
{
    // print for debug
    cout << "Removing route to " << destination << endl;
    table.erase(destination);
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

        
        destID = ntohs(*reinterpret_cast<unsigned short*>(packet.payload[offset]));
        offset += sizeof(router_id);

        nextHop = ntohs(*reinterpret_cast<unsigned short*>(packet.payload[offset]));
        offset += sizeof(router_id);

        routeCost = ntohs(*reinterpret_cast<unsigned short*>(packet.payload[offset]));
        offset += sizeof(cost);

        table.updateRoute(destID, nextHop, routeCost); // updateRoute should already take care of the timestamp stuff
    }
    cout << "DV Payload received. Table contents:" << endl;
    table.printTable();
    
    return table;
}