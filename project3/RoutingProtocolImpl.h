#ifndef ROUTINGPROTOCOLIMPL_H
#define ROUTINGPROTOCOLIMPL_H

#include "RoutingProtocol.h"
#include "Node.h"
#include "LinkState.h"
#include "DistanceVector.h"

class RoutingProtocolImpl : public RoutingProtocol {
  public:
    RoutingProtocolImpl(Node *n);
    ~RoutingProtocolImpl();

    void init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type);
    // As discussed in the assignment document, your RoutingProtocolImpl is
    // first initialized with the total number of ports on the router,
    // the router's ID, and the protocol type (P_DV or P_LS) that
    // should be used. See global.h for definitions of constants P_DV
    // and P_LS.

    void handle_alarm(void *data);
    // As discussed in the assignment document, when an alarm scheduled by your
    // RoutingProtoclImpl fires, your RoutingProtocolImpl's
    // handle_alarm() function will be called, with the original piece
    // of "data" memory supplied to set_alarm() provided. After you
    // handle an alarm, the memory pointed to by "data" is under your
    // ownership and you should free it if appropriate.

    void recv(unsigned short port, void *packet, unsigned short size);
    // When a packet is received, your recv() function will be called
    // with the port number on which the packet arrives from, the
    // pointer to the packet memory, and the size of the packet in
    // bytes. When you receive a packet, the packet memory is under
    // your ownership and you should free it if appropriate. When a
    // DATA packet is created at a router by the simulator, your
    // recv() function will be called for such DATA packet, but with a
    // special port number of SPECIAL_PORT (see global.h) to indicate
    // that the packet is generated locally and not received from 
    // a neighbor router.

    void sendPings();
    void handlePings(unsigned short port, Packet pingPacket);
    void handlePongs(unsigned short port, Packet pongPacket);
    void handleData(unsigned short port, void* dataPacket);
    // Send some pings to neighbors

    void updatePortFreshness();
    // TODO: Implement this method to iterate through ports. Ports that have not received a PONG in the last 15 seconds should have portStatus.isUp set to false.
    //      (this method will be called every 1 second)

 private:
    Node *sys; // To store Node object; used to access GSR9999 interfaces 
    unsigned short routerID; // Router ID
    unsigned short numPorts; // Number of ports on the router
    eProtocolType protocolType; // Protocol type (P_DV or P_LS)
    unordered_map<port_num, PortStatusEntry> portStatus; // Port status map
    unordered_map<router_id, Neighbor> adjacencyList; // Adjacency list
    unordered_map<router_id, router_id> forwardingTable; // Forwarding table

    LinkState myLSRP;
    DistanceVector myDV;
};

#endif

