#include "DistanceVector.h"
#include "sharedUtils.h"
#include <climits>
#include <unordered_set>

DistanceVector::DistanceVector() : sys(nullptr), myRouterID(0), adjacencyList(nullptr), portStatus(nullptr), numPorts(0) {} // Default constructor is unused, but necessary for compilation
DistanceVector::DistanceVector(Node* n, router_id id, adjacencyList_ptr adjList, portStatus_ptr portStatus, port_num numPorts) : sys(n), myRouterID(id), adjacencyList(adjList), portStatus(portStatus), numPorts(numPorts) {}
// DistanceVector::DistanceVector(Node *n, router_id id, adjacencyList_ref adjList, portStatus_ref portStatus, DVForwardingTable forwardingTable, port_num numPorts) : sys(n), myRouterID(id), adjacencyList(adjList), portStatus(portStatus), forwardingTable(forwardingTable), numPorts(numPorts), seqNum(0) {}

// Populate a distance vector packet and set the packet's destination as
// neighborID (since DV packets are only sent to immediate neighbors)
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

            if (nextHop == neighborID && routeDestID != nextHop) { // Poison Reverse: If a node (C) goes through its neighbor (B) to get to a route destination (A), then have C tell B its distance to A is infinite
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
void DistanceVector::handleDVPacket(port_num port, Packet dvPacket) {
    // TODO: PingPong phase needs to provide context for the initial DVs for each node.
    // The PingPong phase should also populate the initial forwarding tables for each node.

    // check staleness of packet (idk how to do this yet)

    // unpack the payload into a DVTable struct
    // DVPacketPayload dvPayload = deserializeDVPayload(dvPacket.payload);
    cout << "Handling DV packet from neighbor " << dvPacket.header.sourceID << endl;
    int neighborID = dvPacket.header.sourceID;
    DVForwardingTable dvPayload = deserializeDVPayload(dvPacket, this->sys);

    // print received forwarding table
    if (verbose) {
        cout << "DV Payload received by Router ID " << this->myRouterID << " from neighbor " << neighborID << ". Table contents:" << endl;
        dvPayload.printTable();
        cout << "Router ID " << this->myRouterID << " Big Table contents:" << endl;
        bigTable.printTable();
    }

    // bellman-ford algorithm
    // iterate thru the table from received packet and update adj list ref

    bool updateRequired = false;
    for (auto row : dvPayload.table)
    {
        auto dest = row.first;
        auto nbrToDestRoute = row.second;
        // ignore routes from neighbors whose destination is themselves
        if (dest == this->myRouterID)
        {
            continue;
        }

        // always update DV big table
        RouteInfo bestRoute = bigTable.getBestRoute(dest); 
        if (nbrToDestRoute.routeCost == USHRT_MAX) {
            // set the route back to USHRT_MAX
            if (bestRoute.routeCost == 0) {
                 bigTable.updateRoute(dest, neighborID, USHRT_MAX, verbose);
            }
        }
        else {
            bigTable.updateRoute(dest, neighborID, (*adjacencyList)[neighborID].timeCost + nbrToDestRoute.routeCost, verbose); 
            if ((*adjacencyList)[neighborID].timeCost + nbrToDestRoute.routeCost < forwardingTable.getRoute(dest).routeCost)
            {
                forwardingTable.updateRoute(dest, neighborID, (*adjacencyList)[neighborID].timeCost + nbrToDestRoute.routeCost, verbose);
                updateRequired = true;
            }
        }
    }

    // case: a neighbor A's link to another neighbor B of theirs is down, so the update isn't going to contain an entry to B anymore
    // if we receive this, we check if the old route to B was through A and update the big table accordingly
    for (auto& pair : bigTable.table) {
        router_id destID = pair.first;
        if (destID == neighborID) { // DV payload coming from a neighbor won't contain a path to itself
            continue;
        }

        // Check if the neighbor exists as a nextHop for this destination
        if (bigTable.table[destID].count(neighborID) > 0) {
            // If the neighbor does exist, make sure the payload DV has a path to destination, if it doesn't then we need to update this path
            if (dvPayload.table.count(destID) == 0) {
                bigTable.removeRoute(destID, neighborID);
                RouteInfo bestRoute = bigTable.getBestRoute(destID);
                if (bestRoute.routeCost != USHRT_MAX - 1) { // if a next best route exists, update the forwarding table
                    forwardingTable.updateRoute(destID, bestRoute.nextHop, bestRoute.routeCost, verbose);
                } else { // if no next best route exists, remove the destination from the forwarding table
                    cout << "about to remove route to " << destID << " so printing table" << endl;
                    forwardingTable.printTable();
                    forwardingTable.removeRoute(destID);
                }
                updateRequired = true;
            }
        }
    }

    // if the table was updated, send out a new DV packet to all neighbors
    if (updateRequired) {
        if (verbose) {
            cout << "Update required, new table is: " << endl;
            forwardingTable.printTable();
        }
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
void DistanceVector::handleCostChange(port_num port, int changeCost)
{
    bool updateRequired = false;
    if (verbose) {
        cout << "Cost change detected for neighbor " << (*portStatus)[port].destRouterID << " on port " << port << endl;
        cout << "current cost: " << (*portStatus)[port].timeCost << ", change: " << changeCost << endl;
        cout << "Old cost: " << (*portStatus)[port].timeCost - changeCost << ", New cost: " << (*portStatus)[port].timeCost << endl;
        cout << "Old table for Router ID: " << this->myRouterID << endl;
        forwardingTable.printTable();
    }
    router_id neighborID = (*portStatus)[port].destRouterID; // Get the neighbor ID from the port

    // Handle case where forwardingTable has never seen this destination before
    if (forwardingTable.table.count(neighborID) == 0) {
        forwardingTable.updateRoute(neighborID, neighborID, 0, verbose); // initialize cost as 0 because below we will add changecost
        bigTable.updateRoute(neighborID, neighborID, 0, verbose); // initialize cost as 0 because below we will add changecost
        updateRequired = true;
    }

    // handle case where bigTable's destination has never seen this nextHop before
    if (bigTable.table[neighborID].count(neighborID) == 0) {
        bigTable.updateRoute(neighborID, neighborID, 0, verbose); // initialize cost as 0 because below we will add changecost
    }

    // Iterate through every destination in the bigTable
    for (auto& pair : bigTable.table) { 
        router_id destination = pair.first;
        unordered_map<router_id, cost>& nextHops = pair.second;
        // Check if the nextHop exists for this destination
        auto hopIterator = nextHops.find(neighborID);
        if (hopIterator != nextHops.end()) {
            // Update the cost for the specified nextHop
            cost& updatedCost = hopIterator->second;
            updatedCost += changeCost;

            // Check if this nextHop is now the minimum cost for the destination
            RouteInfo bestRoute = bigTable.getBestRoute(destination);
            if (bestRoute.nextHop == neighborID) {
                // Update the forwarding table
                forwardingTable.updateRoute(destination, neighborID, updatedCost, verbose);
                updateRequired = true;
            }
        }
    }

    if (verbose) {
        cout << "New table for Router ID: " << this->myRouterID << endl;
        forwardingTable.printTable();
    }

    if (updateRequired) {
        sendUpdates();
    }
}

bool DistanceVector::dvEntryExpiredCheck() {
    // iterate through the forwarding table and remove any entries that have not been updated in the last 45 seconds
    unordered_set<router_id> removeSet;
    for (auto row : forwardingTable.table) {
        router_id destID = row.first;
        ForwardingEntry route = row.second;
        time_stamp currTime = sys->time();
        if (currTime - route.lastUpdate > 45 * 1000) {
            removeSet.insert(destID);
        }
    }
    for (router_id destID : removeSet) {
        forwardingTable.removeRoute(destID);
        (*this->adjacencyList)[destID].timeCost = 0;
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
            cout << "Port to destination " << it->second.destRouterID << " has expired." << endl;
            removeSet.insert(it->second.destRouterID);
        }
    }

    for (router_id nextHop : removeSet) {
        unordered_set<router_id> removedDest = this->forwardingTable.removeRoutesWithNextHop(nextHop);
        this->bigTable.removeRoutesWithNextHop(nextHop);

        // attempt to replace the expired routes with the next best option
        for (router_id dest : removedDest) {
            RouteInfo bestRoute = bigTable.getBestRoute(dest);
            if (bestRoute.nextHop != 0) {
                forwardingTable.updateRoute(dest, bestRoute.nextHop, bestRoute.routeCost, verbose);
            }
        }
        (*this->adjacencyList)[nextHop].timeCost = 0;
    }

    return removeSet.size() > 0;
}
