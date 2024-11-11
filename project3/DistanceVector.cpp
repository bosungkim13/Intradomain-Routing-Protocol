#include "DistanceVector.h"
#include "sharedUtils.h"
#include "dvUtils.h"
#include <climits>

DistanceVector::DistanceVector(Node *n, router_id id, adjacencyList_ref adjList, portStatus_ref portStatus, DVForwardingTable forwardingTable, port_num numPorts) : sys(n), myRouterID(id), adjacencyList(adjList), portStatus(portStatus), forwardingTable(forwardingTable), numPorts(numPorts), seqNum(0) {}

Packet DistanceVector::createDVPacket(unsigned short neighborID)
{

    // To Michael: this is pretty much identical to Bosung's code. the only real difference is
    // that the destID is populated. I also didn't really finish this implementation
    // because I got stuck on deciding how to handle the payload: manual packing of
    // the values or taking advantage of the struct.
    Packet packet;
    packet.header.packetType = DV;
    packet.header.size = HEADER_SIZE + forwardingTable.table.size() * (2 * sizeof(router_id) + sizeof(cost)); // structure of payload entry: destID, nextHop, routeCost
    packet.header.sourceID = this->myRouterID;
    packet.header.destID = neighborID; // note that destID in this context refers to destination of packet (which is different from destID in context of a route!)

    uint32_t offset = 0;

    for (const auto& nbr : forwardingTable.table) { // added const and & because we won't be modifying in the for loop, so more efficient (won't make a copy)
            router_id routeDestID = nbr.first;
            router_id nextHop = nbr.second.nextHop;
            cost routeCost = nbr.second.routeCost;

            if (nextHop == neighborID) { // Poison Reverse: If a node (C) goes through its neighbor (B) to get to a route destination (A), then have C tell B its distance to A is infinite
                routeCost = USHRT_MAX;
            }

            memcpy(packet.payload + offset, &routeDestID, sizeof(router_id));
            offset += sizeof(router_id);
            memcpy(packet.payload + offset, &nextHop, sizeof(router_id));
            offset += sizeof(router_id);
            memcpy(packet.payload + offset, &routeCost, sizeof(cost));
            offset += sizeof(cost);
        }
    return packet;
}

// sends an udpate to all neighbors
void DistanceVector::sendUpdates()
{
    // To Michael: I think the payload of a DV packet should contain the routing table. A neighbors list could also work
    // but I think we already pass that in as reference (adjList_ref). Also it helps that the routing table contains the
    // neighbors already. (same nextHop and dest entries)
    // Michael: D
    for (const auto& port : portStatus)
    {
        PortStatusEntry portStatusEntry = port.second;
        if (portStatusEntry.isUp)
        {
            Packet newDVPacket = createDVPacket(portStatusEntry.destRouterID); 
            void* serializedDVPacket = serializePacket(newDVPacket);
            sys->send(port.first, serializedDVPacket, newDVPacket.header.size);
        }
    }
}

// THIS IS JUST THE "RECEIVING AN UPDATE" CASE.
void DistanceVector::handleDVPacket(port_num port, Packet pongPacket)
{
    // TODO: PingPong phase needs to provide context for the initial DVs for each node.
    // The PingPong phase should also populate the initial forwarding tables for each node.

    // check staleness of packet (idk how to do this yet)

    // unpack the payload into a DVTable struct
    // DVPacketPayload dvPayload = deserializeDVPayload(pongPacket.payload);
    int neighborID = pongPacket.header.sourceID;
    DVForwardingTable dvPayload = deserializeDVPayload(pongPacket, this->sys); 

    // bellman-ford algorithm
    // iterate thru the table from received packet and update adj list ref

    bool updateRequired = false;
    for (auto row : dvPayload.table)
    {
        auto dest = row.first;
        auto nbrToDestRoute = row.second;
        if (adjacencyList[neighborID].timeCost + nbrToDestRoute.routeCost < forwardingTable.getRoute(dest).routeCost)
        {
            forwardingTable.updateRoute(dest, neighborID, adjacencyList[neighborID].timeCost + nbrToDestRoute.routeCost);
            updateRequired = true;
        }
    }

    // if the table was updated, send out a new DV packet to all neighbors
    if (updateRequired)
        sendUpdates();
};

// function that handles changes in neighbor to neighbor cost, determined from
// ping pong process. this is just another part of the DV algorithm
void DistanceVector::handleCostChange(port_num port, cost changeCost)
{

    // get the neighbor ID from the port
    router_id neighborID = portStatus[port].destRouterID;
    bool updateRequired = false;
    adjacencyList[neighborID].timeCost += changeCost; // Note to Daniel: always need to update adjacencyList, since that covers costs for each neighbor

    // update the routing table with the new cost
    for (auto row : forwardingTable.table)
    {
        auto dest = row.first;
        auto route = row.second;
        if (route.nextHop == neighborID)
        {
            // Note to Daniel: no need for the special case where nextHop==dest because changeCost is already the difference between old value and new value
            forwardingTable.updateRoute(dest, neighborID, changeCost + route.routeCost); // TODO: pass in timestamp for freshness check
            updateRequired = true; // since the forwardingTable only contains the least cost paths to destinations, if a route in the table must be updated, then a new min was found => must send updates            
        }
    }
    if (updateRequired)
        sendUpdates();
}

void DistanceVector::updateFreshness() {
    // iterate through the forwarding table and remove any entries that have not been updated in the last 45 seconds
    for (auto row : forwardingTable.table) {
        auto dest = row.first;
        if (!forwardingTable.isFresh(dest)) {
            forwardingTable.removeRoute(dest);
        }
    }
};
