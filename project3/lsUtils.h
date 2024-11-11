#ifndef LSUTILS_H
#define LSUTILS_H

#include <unordered_map>
#include <global.h>
#include <sharedUtils.h>

struct ls_path_info {
    router_id myID;
    time_stamp lastUpdate;
    cost timeCost;
    ls_path_info(router_id id, time_stamp update, cost cost) : myID(id), lastUpdate(update), timeCost(cost) {}
};

#endif