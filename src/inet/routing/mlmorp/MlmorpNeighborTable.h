
// Author: Basheer Al-Qassab

#ifndef __INET_MLMORPNEIGHBORTABLE_H
#define __INET_MLMORPNEIGHBORTABLE_H

#include <map>
#include <vector>

#include "inet/common/geometry/common/Coord.h"
#include "inet/networklayer/common/L3Address.h"

namespace inet {

/**
 * This class provides a mapping between node addresses and neighbors.
 */
class INET_API MlmorpNeighborTable {
private:
    struct Neighbor {
        int networkInterfaceId = -1;
        simtime_t lastUpdate = -1;

        // TODO: add more metrics to the neighbor table
        Coord position = Coord::NIL;
        int nodeDegree = -1;
        double residualEnergy = -1;

        double signalPower = -1;  // Signal power in dBm
        double buffPktNo = -1;         // Signal-to-Noise-plus-Interference Ratio in dB

        // constructor and parameterized constructor for the struct
        Neighbor() {}
        Neighbor(int networkInterfaceId, const Coord &position, int nodeDegree, double residualEnergy, double signalPower, double buffPktNo, simtime_t lastUpdate) :
                networkInterfaceId(networkInterfaceId), position(position), nodeDegree(nodeDegree),
                residualEnergy(residualEnergy), signalPower(signalPower), buffPktNo(buffPktNo), lastUpdate(lastUpdate) {}
    };

    // a container that stores key-value pairs, key is address and value is the neighbor struct
    std::map<L3Address, Neighbor> addressToNeighborMap;

public:
    MlmorpNeighborTable() {}

    std::vector<L3Address> getAddresses() const;

    bool hasNeighbor(const L3Address &address) const;
    int getNetworkInterfaceId(const L3Address &address) const;
    Coord getPosition(const L3Address &address) const;
    void updateNeighbor(const L3Address &address, int networkInterfaceId, const Coord &position, int nodeDegree, double residualEnergy, double signalPower, double buffPktNo);

    simtime_t getOldestNeighbor() const;
    void removeNeighbor(const L3Address &address);
    void removeOldNeighbors(simtime_t timestamp);

    void clear();

    int getNodeDegree(const L3Address &address) const;
    double getResidualEnergy(const L3Address &address) const;

    double getSignalPower(const L3Address &address) const;
    double getBuffPktNo(const L3Address &address) const;

    friend std::ostream& operator<<(std::ostream &o, const MlmorpNeighborTable &t);
};

} // namespace inet

#endif

