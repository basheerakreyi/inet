
// Author: Basheer Al-Qassab

#include "inet/routing/rlmorp/RlmorpNeighborTable.h"
#include "inet/common/stlutils.h"

namespace inet {

// return a vector containing all the neighbor addresses
std::vector<L3Address> RlmorpNeighborTable::getAddresses() const
{
    std::vector<L3Address> addresses;
    for (const auto& elem : addressToNeighborMap)
        addresses.push_back(elem.first);
    return addresses;
}

// check if the node is a neighbor with specified address
bool RlmorpNeighborTable::hasNeighbor(const L3Address& address) const
{
    return containsKey(addressToNeighborMap, address);
}

// return the network interface ID of a specified neighbor
int RlmorpNeighborTable::getNetworkInterfaceId(const L3Address& address) const
{
    auto it = addressToNeighborMap.find(address);
    return (it == addressToNeighborMap.end()) ? -1 : it->second.networkInterfaceId;
}

Coord RlmorpNeighborTable::getPosition(const L3Address& address) const
{
    auto it = addressToNeighborMap.find(address);
    return (it == addressToNeighborMap.end()) ? Coord::NIL : it->second.position;
}

int RlmorpNeighborTable::getNodeDegree(const L3Address& address) const
{
    auto it = addressToNeighborMap.find(address);
    return (it == addressToNeighborMap.end()) ? -1 : it->second.nodeDegree;
}

double RlmorpNeighborTable::getResidualEnergy(const L3Address& address) const
{
    auto it = addressToNeighborMap.find(address);
    return (it == addressToNeighborMap.end()) ? -1 : it->second.residualEnergy;
}

double RlmorpNeighborTable::getBuffPktNo(const L3Address& address) const
{
    auto it = addressToNeighborMap.find(address);
    return (it == addressToNeighborMap.end()) ? -1 : it->second.buffPktNo;
}

double RlmorpNeighborTable::getSignalPower(const L3Address& address) const
{
    auto it = addressToNeighborMap.find(address);
    return (it == addressToNeighborMap.end()) ? -1 : it->second.signalPower;
}

// used to add or update new neighbor to the table
void RlmorpNeighborTable::updateNeighbor(const L3Address& address, int networkInterfaceId, const Coord& position, int nodeDegree, double residualEnergy, double signalPower, double buffPktNo)
{
    ASSERT(!address.isUnspecified());
    addressToNeighborMap[address] = Neighbor(networkInterfaceId, position, nodeDegree, residualEnergy, signalPower, buffPktNo, simTime());
}

// to get the time of the oldest neighbor in the table (the less time the oldest)
simtime_t RlmorpNeighborTable::getOldestNeighbor() const
{
    simtime_t oldestNeighborTime = SimTime::getMaxTime();
    for (const auto& elem : addressToNeighborMap) {
        const simtime_t& time = elem.second.lastUpdate;
        if (time < oldestNeighborTime)
            oldestNeighborTime = time;
    }
    return oldestNeighborTime;
}

// remove the neighbor with a specified address
void RlmorpNeighborTable::removeNeighbor(const L3Address& address)
{
    auto it = addressToNeighborMap.find(address);
    addressToNeighborMap.erase(it);
}

// purge the neighbor table based on the provided time
void RlmorpNeighborTable::removeOldNeighbors(simtime_t timestamp)
{
    for (auto it = addressToNeighborMap.begin(); it != addressToNeighborMap.end();)
        if (it->second.lastUpdate <= timestamp)
            addressToNeighborMap.erase(it++);
        else
            it++;

}

// remove all the neighbor in the neighbor table
void RlmorpNeighborTable::clear()
{
    addressToNeighborMap.clear();
}

// This function is used to show the neighbor table in info
std::ostream& operator<<(std::ostream& o, const RlmorpNeighborTable& t)
{
    o << "{ ";
    for (auto elem : t.addressToNeighborMap) {
        o << elem.first << "@" << elem.second.lastUpdate << ": POS:" << elem.second.position
                << ", NeiD:" << elem.second.nodeDegree << ", ReE:" << elem.second.residualEnergy
                << ", SigP:" << elem.second.signalPower << ", BUFF:" << elem.second.buffPktNo << ";\n";
    }
    o << "}";
    return o;
}

} // namespace inet

