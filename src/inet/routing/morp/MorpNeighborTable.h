
// Author: Basheer Al-Qassab

#ifndef __INET_MORPNEIGHBORTABLE_H
#define __INET_MORPNEIGHBORTABLE_H

#include <map>
#include <vector>

#include "inet/common/geometry/common/Coord.h"
#include "inet/networklayer/common/L3Address.h"

namespace inet {

/**
 * This class provides a mapping between node addresses and neighbors.
 */
class INET_API MorpNeighborTable {
private:
    struct Neighbor {
        int networkInterfaceId = -1;
        simtime_t lastUpdate = -1;

        // TODO: add more metrics to the neighbor table
        Coord position = Coord::NIL;
        int nodeDegree = -1;

        // constructor and parameterized constructor for the struct
        Neighbor() {}
        Neighbor(int networkInterfaceId, const Coord &position, int nodeDegree, simtime_t lastUpdate) :
                networkInterfaceId(networkInterfaceId), position(position), nodeDegree(nodeDegree), lastUpdate(lastUpdate) {}
    };

    // a container that stores key-value pairs, key is address and value is the neighbor struct
    std::map<L3Address, Neighbor> addressToNeighborMap;

public:
    MorpNeighborTable() {}

    std::vector<L3Address> getAddresses() const;

    bool hasNeighbor(const L3Address &address) const;
    int getNetworkInterfaceId(const L3Address &address) const;
    Coord getPosition(const L3Address &address) const;
    void updateNeighbor(const L3Address &address, int networkInterfaceId, const Coord &position, int nodeDegree);

    simtime_t getOldestNeighbor() const;
    void removeNeighbor(const L3Address &address);
    void removeOldNeighbors(simtime_t timestamp);

    void clear();

    int getNodeDegree(const L3Address &address) const;

    friend std::ostream& operator<<(std::ostream &o, const MorpNeighborTable &t);
};

} // namespace inet

#endif

