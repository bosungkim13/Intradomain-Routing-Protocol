#include "DistanceVector.h"
#include <climits>
#include <unordered_set>

DistanceVector::DistanceVector() : sys(nullptr), myRouterID(0), adjacencyList(nullptr), portStatus(nullptr), numPorts(0) {} // Default constructor is unused, but necessary for compilation
DistanceVector::DistanceVector(Node* n, router_id id, adjacencyList_ptr adjList, portStatus_ptr portStatus, port_num numPorts) : sys(n), myRouterID(id), adjacencyList(adjList), portStatus(portStatus), numPorts(numPorts) {}
// DistanceVector::DistanceVector(Node *n, router_id id, adjacencyList_ref adjList, portStatus_ref portStatus, DVForwardingTable forwardingTable, port_num numPorts) : sys(n), myRouterID(id), adjacencyList(adjList), portStatus(portStatus), forwardingTable(forwardingTable), numPorts(numPorts), seqNum(0) {}

// Populate a distance vector packet and set the packet's destination as
// neighborID (since DV packets are only sent to immediate neighbors)
// NOTE: values are still in host order (serializePacket will convert endianness to network order)
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
    packet.header.destID = neighborID; // DV packets are just sent to neighbors

    uint32_t offset = 0;

    // Serialize relevant information from forwarding table into packet's payload
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
    for (const auto& port : *portStatus)
    {
        PortStatusEntry portStatusEntry = port.second;
        if (portStatusEntry.isUp)
        {
            Packet dvPacket = createDVPacket(portStatusEntry.destRouterID); // set destination as neighborID
            void* serializedDVPacket = serializePacket(dvPacket);
            sys->send(port.first, serializedDVPacket, dvPacket.header.size);
        }
    }
}

// THIS IS JUST THE "RECEIVING AN UPDATE" CASE.
void DistanceVector::handleDVPacket(port_num port, Packet dvPacket)
{
    // TODO: PingPong phase needs to provide context for the initial DVs for each node.
    // The PingPong phase should also populate the initial forwarding tables for each node.

    // check staleness of packet (idk how to do this yet)

    // unpack the payload into a DVTable struct
    // DVPacketPayload dvPayload = deserializeDVPayload(dvPacket.payload);
    int neighborID = dvPacket.header.sourceID;
    DVForwardingTable dvPayload = deserializeDVPayload(dvPacket, this->sys);

    // print received forwarding table
    cout << "DV Payload received. Table contents:" << endl;
    dvPayload.printTable();

    // bellman-ford algorithm
    // iterate thru the table from received packet and update adj list ref

    bool updateRequired = false;
    for (auto row : dvPayload.table)
    {
        auto dest = row.first;
        auto nbrToDestRoute = row.second;
        if ((*adjacencyList)[neighborID].timeCost + nbrToDestRoute.routeCost < forwardingTable.getRoute(dest).routeCost)
        {
            // TODO: add another check to see if the destID is associated with any neighbor in adjacencyList. If so, verify the port is alive.
            forwardingTable.updateRoute(dest, neighborID, (*adjacencyList)[neighborID].timeCost + nbrToDestRoute.routeCost);
            updateRequired = true;
        }
    }

    // if the table was updated, send out a new DV packet to all neighbors
    if (updateRequired) {
        cout << "Update required, new table is: " << endl;
        forwardingTable.printTable();
        sendUpdates();
    }
};

// function that handles changes in neighbor to neighbor cost, determined from
// ping pong process. this is just another part of the DV algorithm.
// Parameters: 
//  - port is the number that the PONG came from
//  - changeCost is the difference in timeCost between previous and current RTT.
//      If the port was previously offline, this will simply be the new RTT
//  Note: no need to update timestamp in adjacencyList or portStatus since 
//  those have been updated in RoutingProtocolImpl.cc before delegating to this
void DistanceVector::handleCostChange(port_num port, cost changeCost)
{
    cout << "Cost change detected! Old cost: " << (*portStatus)[port].timeCost - changeCost << ", New cost: " << (*portStatus)[port].timeCost << endl;
    cout << "Old table for Router ID: " << this->myRouterID << endl;
    forwardingTable.printTable();
    router_id neighborID = (*portStatus)[port].destRouterID; // Get the neighbor ID from the port

    // Update any routing table entries that use this link (uses neighborID as nextHop) with the new cost
    for (auto row : forwardingTable.table)
    {
        auto dest = row.first;
        auto route = row.second;
        if (route.nextHop == neighborID)
        {
            forwardingTable.updateRoute(dest, neighborID, changeCost + route.routeCost); 
        }
    }

    // Handle case where forwardingTable has never seen this destination before
    if (forwardingTable.table.find(neighborID) == forwardingTable.table.end()) {
        forwardingTable.updateRoute(neighborID, neighborID, changeCost); 
    }

    cout << "New table for Router ID: " << this->myRouterID << endl;
    forwardingTable.printTable();
}

bool DistanceVector::dvEntryExpiredCheck() {
    // iterate through the forwarding table and remove any entries that have not been updated in the last 45 seconds
    unordered_set<router_id> removeSet;
    for (auto row : forwardingTable.table) {
        router_id destID = row.first;
        DVRoute route = row.second;
        time_stamp currTime = sys->time();
        if (currTime - route.lastUpdate > 45 * 1000) {
            removeSet.insert(destID);
        }
    }
    for (router_id destID : removeSet) {
        forwardingTable.removeRoute(destID);
    }

    // print destination that expired
    for (router_id destID : removeSet) {
        cout << "Destination " << destID << " has expired and will be removed." << endl;
    }

    return removeSet.size() > 0;
};

bool DistanceVector::portExpiredCheck() {
    bool expired = false;
    unordered_set<router_id> removeSet;
    for (auto it = ((*this->portStatus).begin)(); it != (*this->portStatus).end(); it++) {
        if (this->sys->time() - it->second.lastUpdate > 15 * 1000) {
            it->second.timeCost = INFINITY_COST;
            it->second.isUp = false;
            removeSet.insert(it->second.destRouterID);
        }
    }

    for (router_id destID : removeSet) {
        this->forwardingTable.removeRoute(destID);
    }

    return removeSet.size() > 0;
}
