#include "RoutingProtocolImpl.h"
#include "global.h"
#include <sys/time.h>
#include "sharedUtils.h"
#include <cassert>

RoutingProtocolImpl::RoutingProtocolImpl(Node *n) : RoutingProtocol(n) {
  sys = n;
  // add your own code
}

RoutingProtocolImpl::~RoutingProtocolImpl() {
  // Note: We don't need to delete sys as it's owned by the simulator
  // Note: myLSRP and myDV are stack-allocated objects that will be cleaned up automatically
  // Note: The unordered_maps will clean up their contents automatically
}

void RoutingProtocolImpl::init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type) {
  this->numPorts = num_ports;
  this->routerID = router_id;
  this->protocolType = protocol_type;

  char *ppAlarm = new char[sizeof(eAlarmType)];
  char *updateAlarm = new char[sizeof(eAlarmType)];
  char *freshnessAlarm = new char[sizeof(eAlarmType)];

  *((eAlarmType *)ppAlarm) = PingPongAlarm;
  *((eAlarmType *)updateAlarm) = UpdateAlarm;
  *((eAlarmType *)freshnessAlarm) = FreshnessAlarm;

  sendPings();

  this->sys->set_alarm(this, 1 * 1000, freshnessAlarm);
  this->sys->set_alarm(this, 10 * 1000, ppAlarm);
  this->sys->set_alarm(this, 30 * 1000, updateAlarm);

  // initialize link or distance vector protocol
  if (this->protocolType == P_LS) {
    this->myLSRP = LinkState(this->sys, this->routerID, &this->adjacencyList, &this->portStatus, &this->forwardingTable, this->numPorts);
  } else if (this->protocolType == P_DV) {
    this->myDV = DistanceVector(this->sys, this->routerID, &this->adjacencyList, &this->portStatus, this->numPorts);
  }

}

void RoutingProtocolImpl::handle_alarm(void *data) {
  // add your own code
  eAlarmType type = (*((eAlarmType *)data));
  switch (type) {
    case PingPongAlarm:
      this->sendPings();
      this->sys->set_alarm(this, 10 * 1000, data);
      break;
    case UpdateAlarm:
      if (protocolType == P_LS) {
        // LS
        this->myLSRP.SendUpdates();
      } else {
        // DV
        this->myDV.sendUpdates();
      }
      this->sys->set_alarm(this, 30 * 1000, data);
      break;
    case FreshnessAlarm:
      // check link every second
      if (protocolType == P_LS) {
        if (this->myLSRP.PortExpiredCheck()) {
          // port expired check updates the table so send updates doesn't send to dead ports
          this->myLSRP.SendUpdates();
        }

        // update freshness
        if (this->myLSRP.NodeTableExpiredCheck()) {
          // updates routing table based on current link state table
          this->myLSRP.UpdateTable();
        }

      } else {
        // DV

        // update port freshness
        if (this->myDV.portExpiredCheck()) {
          // port expired check updates the ports table so send updates doesn't send to dead ports
          this->myDV.sendUpdates();
        }

        // update DV entry freshness
        if (this->myDV.dvEntryExpiredCheck()) {
          this->myDV.sendUpdates();
        }
      }
      this->sys->set_alarm(this, 1 * 1000, data);
      break;
    default:
      if (verbose) std::cout << "handle_alarm(): Unknown alarm type." << std::endl;
      break;
  }
}

void RoutingProtocolImpl::recv(unsigned short port, void *packet, unsigned short size) {
  // add your own code
  Packet deserializedPacket = deserializePacket(packet);
  switch (deserializedPacket.header.packetType) {
    case PING:
      // Handle PING packet
      * (time_stamp *) deserializedPacket.payload = ntohl(*(time_stamp *) deserializedPacket.payload);
      this->handlePings(port, deserializedPacket);
      free(packet); // free the received packet's memory bc it won't be re-used
      break;
    case PONG:
      // Handle PONG packet
      this->handlePongs(port, deserializedPacket);
      free(packet); // free the received packet's memory bc it won't be re-used
      break;
    case DATA:
      // Handle DATA packet
      this->handleData(port, packet);
      // Can't free the received packet's memory because it gets reused when forwarding the packet
      break;
    case LS:
      // Handle LS packet
      this->myLSRP.HandlePacket(port, packet, size);
      // LinkState.HandlePacket() frees the received packet's memory, no need to call free here
      break;
    case DV:
      // Handle DV packet
      this->myDV.handleDVPacket(port, deserializedPacket);
      free(packet); // free the received packet's memory bc it won't be re-used
      break;
    default:
      // Handle unknown packet type
      break;
  }
}

// add more of your own code

// HELPER FUNCTIONS
// Function to send Ping-Pong packets
void RoutingProtocolImpl::sendPings() {
  int size = HEADER_SIZE + sizeof(time_stamp); // Size of the packet struct

  for (unsigned int i = 0; i < this->numPorts; i++) {
    Packet packet;
    time_stamp timestamp = sys->time(); // Set timestamp
    memcpy(packet.payload, &timestamp, sizeof(timestamp)); // Copy timestamp to payload
    PacketHeader *packetHeader = &packet.header;
    packetHeader->packetType = PING; // Set packet type
    packetHeader->size = size; // Set packet size
    packetHeader->sourceID = this->routerID; // Set source ID

    // Destination packet is unused in the ping packet, so michael commented this out:
    // // defaults to 0 if not set
    // packetHeader->destID = htons(portStatus[i].destRouterID); // Set destination ID

    // Serialize
    assert(packet.header.packetType == PING);
    assert(packet.header.size == HEADER_SIZE + sizeof(time_stamp));
    void *serializedPacket = serializePacket(packet);
    // Send the packet
    sys->send(i, serializedPacket, size);
  }
}

/*
When the neighbor router receives the PING message, it must update the 
received message’s type to PONG, copy the source ID to the destination ID,
update the source ID to its own, then send the resulting PONG message (with the
original timestamp still in the payload) immediately back to the neighbor.

'port' is the port from which this packet was received.
*/
void RoutingProtocolImpl::handlePings(unsigned short port, Packet pingPacket) {
  assert(pingPacket.header.packetType == PING);
  pingPacket.header.packetType = PONG;
  pingPacket.header.destID = pingPacket.header.sourceID;
  pingPacket.header.sourceID = this->routerID;
  assert(pingPacket.header.packetType == PONG);
  void *serializedPongPacket = serializePacket(pingPacket);
  sys->send(port, serializedPongPacket, pingPacket.header.size);
}

/*
When the PONG message is received, the timestamp in the message is compared 
to the current time to compute the RTT. 

'port' is the port from which this packet was received.
*/
void RoutingProtocolImpl::handlePongs(unsigned short port, Packet pongPacket) {
  // Calculate RTT
  time_stamp prevTimestamp = ntohl(*(time_stamp*)pongPacket.payload); // TODO: how did removing ntohl fix it????
  time_stamp currTimestamp = sys->time();
  if (verbose) {
    cout << "prevTimestamp: " << prevTimestamp << endl;
    cout << "currTimestamp: " << currTimestamp << endl;
  }
  // assert(prevTimestamp <= currTimestamp);
  time_stamp rtt = currTimestamp - prevTimestamp;

  // Use the PONG packet's source ID to discover the ID of its current neighbor
  router_id destID = pongPacket.header.sourceID;

  // If we didn't previously know port, create port -> PortStatusEntry mapping 
  if (portStatus.find(port) == portStatus.end()) {
    portStatus[port] = PortStatusEntry();
  }

  // Update that port's information
  portStatus[port].destRouterID = destID;
  bool wasUp = portStatus[port].isUp;
  portStatus[port].isUp = true;
  portStatus[port].lastUpdate = currTimestamp;
  portStatus[port].timeCost = rtt;

  if (protocolType == P_DV) {

    // If we didn't previously have destID as a neighbor, create destID -> neighbor (port, timecost) mapping 
    if (adjacencyList.find(destID) == adjacencyList.end()) {
      adjacencyList[destID] = Neighbor(port, 0);
    }

    adjacencyList[pongPacket.header.sourceID].port = port;
    cost oldTimeCost = adjacencyList[pongPacket.header.sourceID].timeCost;  // Note: if the destID has just been added to adjList, timeCost will be 0
    adjacencyList[pongPacket.header.sourceID].timeCost = rtt;
    // for DV, call "handleCostChange()" whether or not timecost changed. This is because it still needs to update "lastUpdated" time stamp for each DV entry associated with this link
    int changeCost = adjacencyList[pongPacket.header.sourceID].timeCost - oldTimeCost;
    this->myDV.handleCostChange(port, changeCost);
    // send updates to all neighbors
    // this->myDV.sendUpdates(); // Commented out because we only need to sendUpdates when forwarding table changes (and handleCostChange() handles this!)
  } else if (protocolType == P_LS) {
    // preserving the behavior 
    bool topologyChanged = false;
    bool rttChanged = false;
    if (this->adjacencyList.find(destID) != this->adjacencyList.end() && wasUp) {
      // If this port was previously up, only send updates if time cost changed
      this->adjacencyList[destID].port = port;
      cost oldTimeCost = this->adjacencyList[destID].timeCost;
      this->adjacencyList[destID].timeCost = rtt;
      this->myLSRP.RefreshNodeEntry(destID);

      rttChanged = oldTimeCost != this->adjacencyList[destID].timeCost;
      if (rttChanged) {
        std::cout << "handlePongs() for " << this->routerID << ": rtt changed for neighbor " << destID << " on port " << port << std::endl;
      }
    } else {
      // Update the forwarding table? Should always be the same as
      this->adjacencyList[destID] = Neighbor(port, rtt);
      topologyChanged = true;
      std::cout << "handlePongs() for " << this->routerID << ": topology changed for neighbor " << destID << " on port " << port << std::endl;
    }
    if (topologyChanged || rttChanged) {
      // If port was not previously up, send updates no matter what
      // for link state routing protocol, we need to update since cost changes
      this->myLSRP.UpdateTable();
      // send updates to all neighbors
      this->myLSRP.SendUpdates();
    }
  }
}

// This method will be called every time a DATA packet is received.
void RoutingProtocolImpl::handleData(unsigned short port, void* handleMe) {
  Packet dataPacket = deserializePacket(handleMe);
  dataPacket.header.packetType = DATA;
  router_id destId = dataPacket.header.destID;

  // TODO: Implement handling for when data packet origniates from this router
  if (port == SPECIAL_PORT) {
    // dataPacket.header.sourceID = this->routerID;
    if (verbose) std::cout << "handleData(): Data packet originates from router "  << dataPacket.header.sourceID << " going to router " << dataPacket.header.destID << std::endl;
    // handleMe = serializePacket(dataPacket);
  }


  if (dataPacket.header.packetType != DATA) {
    if (verbose) std::cout << "handleData(): Packet type is not DATA." << std::endl;
  }

  if (destId == this->routerID) {
    if (verbose) std::cout << "handleData(): destId == this->routerID." << std::endl;
    delete[] static_cast<char*>(handleMe);
    return;
  }

  // this is poor design, but DV has its own separate instance of a forwarding table, so we need to use that one.
  if (this->protocolType == P_LS) {
    if (this->forwardingTable.find(destId) != this->forwardingTable.end()) {
      // If the destination is in the forwarding table, send the packet to the next hop
      router_id nextHopRouterID = this->forwardingTable[destId];      
      sys->send(this->adjacencyList[nextHopRouterID].port, handleMe, dataPacket.header.size);
    }
  } else if (this->protocolType == P_DV) { 
    if (this->myDV.forwardingTable.table.count(destId) != 0) {
      router_id nextHopRouterID = this->myDV.forwardingTable.table[destId].nextHop;
      port_num nextHopPort = adjacencyList[nextHopRouterID].port;
      if (verbose) {
        cout << "About to send packet to next hop" << endl;
        cout << "Sending it from router ID " << this->routerID << " to nextHopRouterID " << this->myDV.forwardingTable.table[destId].nextHop << " and nextHopPort " << nextHopPort << endl;
      }
      // If the destination is in the forwarding table, send the packet to the next hop's port (not the router id!)
      sys->send(nextHopPort, handleMe, dataPacket.header.size);
    }
  }
}