
// Author: Basheer Al-Qassab

#include "inet/routing/morp/MorpNeighborTable.h"
#include "inet/common/stlutils.h"

namespace inet {

// return a vector containing all the neighbor addresses
std::vector<L3Address> MorpNeighborTable::getAddresses() const
{
    std::vector<L3Address> addresses;
    for (const auto& elem : addressToNeighborMap)
        addresses.push_back(elem.first);
    return addresses;
}

// check if the node is a neighbor with specified address
bool MorpNeighborTable::hasNeighbor(const L3Address& address) const
{
    return containsKey(addressToNeighborMap, address);
}

// return the network interface ID of a specified neighbor
int MorpNeighborTable::getNetworkInterfaceId(const L3Address& address) const
{
    auto it = addressToNeighborMap.find(address);
    return (it == addressToNeighborMap.end()) ? -1 : it->second.networkInterfaceId;
}

Coord MorpNeighborTable::getPosition(const L3Address& address) const
{
    auto it = addressToNeighborMap.find(address);
    return (it == addressToNeighborMap.end()) ? Coord::NIL : it->second.position;
}

int MorpNeighborTable::getNodeDegree(const L3Address& address) const
{
    auto it = addressToNeighborMap.find(address);
    return (it == addressToNeighborMap.end()) ? -1 : it->second.nodeDegree;
}

double MorpNeighborTable::getResidualEnergy(const L3Address& address) const
{
    auto it = addressToNeighborMap.find(address);
    return (it == addressToNeighborMap.end()) ? -1 : it->second.residualEnergy;
}

// used to add or update new neighbor to the table
void MorpNeighborTable::updateNeighbor(const L3Address& address, int networkInterfaceId, const Coord& position, int nodeDegree, double residualEnergy)
{
    ASSERT(!address.isUnspecified());
    addressToNeighborMap[address] = Neighbor(networkInterfaceId, position, nodeDegree, residualEnergy, simTime());
}

// to get the time of the oldest neighbor in the table (the less time the oldest)
simtime_t MorpNeighborTable::getOldestNeighbor() const
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
void MorpNeighborTable::removeNeighbor(const L3Address& address)
{
    auto it = addressToNeighborMap.find(address);
    addressToNeighborMap.erase(it);
}

// purge the neighbor table based on the provided time
void MorpNeighborTable::removeOldNeighbors(simtime_t timestamp)
{
    for (auto it = addressToNeighborMap.begin(); it != addressToNeighborMap.end();)
        if (it->second.lastUpdate <= timestamp)
            addressToNeighborMap.erase(it++);
        else
            it++;

}

// remove all the neighbor in the neighbor table
void MorpNeighborTable::clear()
{
    addressToNeighborMap.clear();
}

// This function is used to show the neighbor table in info
std::ostream& operator<<(std::ostream& o, const MorpNeighborTable& t)
{
    o << "{ ";
    for (auto elem : t.addressToNeighborMap) {
        o << elem.first << "@" << elem.second.lastUpdate << ":" << elem.second.position << ", " << elem.second.nodeDegree << ", " << elem.second.residualEnergy << "; ";
    }
    o << "}";
    return o;
}

} // namespace inet

