
// Author: Basheer Al-Qassab

#include "inet/routing/mmlrp/MmlrpNeighborTable.h"
#include "inet/common/stlutils.h"

namespace inet {

std::vector<L3Address> MmlrpNeighborTable::getAddresses() const
{
    std::vector<L3Address> addresses;
    for (const auto& elem : addressToNeighborMap)
        addresses.push_back(elem.first);
    return addresses;
}

bool MmlrpNeighborTable::hasNeighbor(const L3Address& address) const
{
    return containsKey(addressToNeighborMap, address);
}

int MmlrpNeighborTable::getNetworkInterfaceId(const L3Address& address) const
{
    auto it = addressToNeighborMap.find(address);
    return (it == addressToNeighborMap.end()) ? -1 : it->second.networkInterfaceId;
}

Coord MmlrpNeighborTable::getPosition(const L3Address& address) const
{
    auto it = addressToNeighborMap.find(address);
    return (it == addressToNeighborMap.end()) ? Coord::NIL : it->second.position;
}

void MmlrpNeighborTable::updateNeighbor(const L3Address& address, int networkInterfaceId, const Coord& position)
{
    ASSERT(!address.isUnspecified());
    addressToNeighborMap[address] = Neighbor(networkInterfaceId, position, simTime());
}

// to get the time of the oldest neighbor in the table (the less time the oldest)
simtime_t MmlrpNeighborTable::getOldestNeighbor() const
{
    simtime_t oldestNeighborTime = SimTime::getMaxTime();
    for (const auto& elem : addressToNeighborMap) {
        const simtime_t& time = elem.second.lastUpdate;
        if (time < oldestNeighborTime)
            oldestNeighborTime = time;
    }
    return oldestNeighborTime;
}

void MmlrpNeighborTable::removeNeighbor(const L3Address& address)
{
    auto it = addressToNeighborMap.find(address);
    addressToNeighborMap.erase(it);
}

void MmlrpNeighborTable::removeOldNeighbors(simtime_t timestamp)
{
    for (auto it = addressToNeighborMap.begin(); it != addressToNeighborMap.end();)
        if (it->second.lastUpdate <= timestamp)
            addressToNeighborMap.erase(it++);
        else
            it++;

}

void MmlrpNeighborTable::clear()
{
    addressToNeighborMap.clear();
}

std::ostream& operator<<(std::ostream& o, const MmlrpNeighborTable& t)
{
    o << "{ ";
    for (auto elem : t.addressToNeighborMap) {
        o << elem.first << ":(" << elem.second.lastUpdate << ";" << elem.second.position << ") ";
    }
    o << "}";
    return o;
}

} // namespace inet

