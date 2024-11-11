#include "RoutingProtocolImpl.h"
#include <global.h>
#include <sys/time.h>
#include <sharedUtils.h>
#include <cassert>

RoutingProtocolImpl::RoutingProtocolImpl(Node *n) : RoutingProtocol(n) {
  sys = n;
  // add your own code
}

RoutingProtocolImpl::~RoutingProtocolImpl() {
  // add your own code (if needed)
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

  // TODO: initialize link or distance vector protocol

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
      break;
    case FreshnessAlarm:
      break;
    default:
      std::cout << "handle_alarm(): Unknown alarm type." << std::endl;
      break;
  }
}

void RoutingProtocolImpl::recv(unsigned short port, void *packet, unsigned short size) {
  // add your own code
  Packet deserializedPacket = deserializePacket(packet);
  switch (deserializedPacket.header.packetType) {
    case PING:
      // Handle PING packet
      this->handlePings(port, deserializedPacket);
      break;
    case PONG:
      // Handle PONG packet
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
    time_stamp timestamp = htonl(sys->time()); // Set timestamp
    memcpy(packet.payload, &timestamp, sizeof(timestamp)); // Copy timestamp to payload

    PacketHeader *packetHeader = &packet.header;
    packetHeader->packetType = htons(PING); // Set packet type
    packetHeader->size = htons(size); // Set packet size
    packetHeader->sourceID = htons(routerID); // Set source ID

    // Destination packet is unused in the ping packet, so michael commented this out:
    // // defaults to 0 if not set
    // packetHeader->destID = htons(portStatus[i].destRouterID); // Set destination ID

    // Serialize
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
  pingPacket.header.packetType = PONG;
  pingPacket.header.destID = pingPacket.header.sourceID;
  pingPacket.header.sourceID = this->routerID;

  void *serializedPongPacket = serializePacket(pingPacket);
  sys->send(port, serializedPongPacket, sizeof(Packet));
}

/*
When the PONG message is received, the timestamp in the message is compared 
to the current time to compute the RTT. 

'port' is the port from which this packet was received.
*/
void RoutingProtocolImpl::handlePongs(unsigned short port, Packet pongPacket) {
  // Calculate RTT
  time_stamp prevTimestamp = ntohl(*(time_stamp*)pongPacket.payload);
  time_stamp currTimestamp = sys->time();
  // assert(prevTimestamp <= currTimestamp);
  time_stamp rtt = currTimestamp - prevTimestamp;
  std::cout << "handlePongs(): RTT = " << rtt << std::endl;

  // Use the PONG packet's source ID to discover the ID of its current neighbor and update that port's information
  portStatus[port].destRouterID = pongPacket.header.sourceID;
  bool wasUp = portStatus[port].isUp;
  portStatus[port].isUp = true;
  portStatus[port].lastUpdate = currTimestamp;
  portStatus[port].timeCost = rtt;

  // check if we have connected before
  if (adjacencyList.count(pongPacket.header.sourceID) && wasUp) {
    // If the neighbor is already in the adjacency list, update the time cost
    adjacencyList[pongPacket.header.sourceID].port = port;
    cost oldTimeCost = adjacencyList[pongPacket.header.sourceID].timeCost;
    adjacencyList[pongPacket.header.sourceID].timeCost = rtt;
    std::cout << "handlePongs(): oldTimeCost = " << oldTimeCost << std::endl;
    std::cout << "handlePongs(): newTimeCost = " << adjacencyList[pongPacket.header.sourceID].timeCost << std::endl;

    if (adjacencyList[pongPacket.header.sourceID].timeCost != oldTimeCost) {
      // Time cost has changed

      if (protocolType == P_LS) {
          // for link state routing protocol, we need to update since cost changes
          // myLSRP->updateTable();
          // send updates to all neighbors
          // myLSRP->sendUpdates();
      }

    } else {
      // Time cost has not changed
      adjacencyList[pongPacket.header.sourceID]= Neighbor(port, rtt);

      if (protocolType == P_LS) {
        // myLSRP->updateTable();
        // myLSRP->sendUpdates();
      }

      // Update the forwarding table? Should alwasy be the same
      assert(forwardingTable[port] == pongPacket.header.sourceID);
      forwardingTable[port] = pongPacket.header.sourceID;

    }

  }
}