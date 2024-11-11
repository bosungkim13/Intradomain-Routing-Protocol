#include "sharedUtils.h"


// Handle DV update packet received from a neighbor 
//  This should involve updating DVForwardingTable struct (and I think it should send updated DV to neighbors?)
//      (this method will be called each time neighbor a neighbor periodically sends a DV packet every 30 seconds)
void handleDVPacket(unsigned short port, Packet pongPacket);


// Iterate through DV entries and remove those that have not been updated in the last 45 seconds
//      (this method will be called every 1 second)
void updateDVFreshness(); 