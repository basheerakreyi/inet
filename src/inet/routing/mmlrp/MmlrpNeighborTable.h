
// Author: Basheer Al-Qassab

#ifndef __INET_MMLRPNEIGHBORTABLE_H
#define __INET_MMLRPNEIGHBORTABLE_H

#include <map>
#include <vector>

#include "inet/common/geometry/common/Coord.h"
#include "inet/networklayer/common/L3Address.h"

namespace inet {

/**
 * This class provides a mapping between node addresses and neighbors.
 */
class INET_API MmlrpNeighborTable {
private:
    struct Neighbor {
        int networkInterfaceId = -1;
        simtime_t lastUpdate = -1;

        // TODO: add more metrics to the neighbor table
        Coord position = Coord::NIL;

        // constructor and parameterized constructor for the struct
        Neighbor() {}
        Neighbor(int networkInterfaceId, const Coord &position, simtime_t lastUpdate) :
                networkInterfaceId(networkInterfaceId), position(position), lastUpdate(lastUpdate) {}
    };

    // a container that stores key-value pairs, key is address and value is the neighbor struct
    std::map<L3Address, Neighbor> addressToNeighborMap;

public:
    MmlrpNeighborTable() {}

    std::vector<L3Address> getAddresses() const;

    bool hasNeighbor(const L3Address &address) const;
    int getNetworkInterfaceId(const L3Address &address) const;
    Coord getPosition(const L3Address &address) const;
    void updateNeighbor(const L3Address &address, int networkInterfaceId, const Coord &position);

    simtime_t getOldestNeighbor() const;
    void removeNeighbor(const L3Address &address);
    void removeOldNeighbors(simtime_t timestamp);


    void clear();

    friend std::ostream& operator<<(std::ostream &o, const MmlrpNeighborTable &t);
};

} // namespace inet

#endif

