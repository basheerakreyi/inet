
// Author: Basheer Al-Qassab

#ifndef __INET_MORP_H
#define __INET_MORP_H

// General INET includes
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/contract/IInterfaceTable.h"
#include "inet/networklayer/contract/ipv4/Ipv4Address.h"
#include "inet/networklayer/ipv4/IIpv4RoutingTable.h"
#include "inet/networklayer/ipv4/Ipv4Header_m.h"
#include "inet/networklayer/ipv4/Ipv4InterfaceData.h"
#include "inet/networklayer/ipv4/Ipv4RoutingTable.h"
#include "inet/routing/base/RoutingProtocolBase.h"

// Include for mobility
#include "inet/common/geometry/common/Coord.h"
#include "inet/mobility/contract/IMobility.h"

// Include for Energy
#include "inet/power/contract/IEpEnergyStorage.h"

// Include for NodeStaus
#include "inet/common/lifecycle/NodeStatus.h"

// Internal includes
#include "inet/routing/morp/Morp_m.h"
#include "inet/routing/morp/MorpRouteData.h"
#include "inet/routing/morp/MorpNeighborTable.h"

namespace inet {

/**
 * MORP protocol implementation.
 */
class INET_API Morp : public RoutingProtocolBase, public cListener
{

private:
    cMessage *event = nullptr;
    cPar *broadcastDelay = nullptr;

    unsigned int sequenceNumber = 0;
    simtime_t routeLifetime;

    //Context
    cModule *host = nullptr;
    NetworkInterface *interface80211ptr = nullptr;
    int interfaceId = -1;
    opp_component_ptr<IMobility> mobility;
    opp_component_ptr<power::IEpEnergyStorage> energyStorage;
    opp_component_ptr<NodeStatus> nodeStatus;

    // Internal
    MorpNeighborTable neighborTable;

protected:
    simtime_t beaconInterval;
    ModuleRefByPar<IInterfaceTable> ift;
    ModuleRefByPar<IIpv4RoutingTable> rt;

public:
    Morp();
    ~Morp();

protected:
    // Initialization function
    virtual void initialize(int stage) override;

    // Operation of the function. Handling Messages
    virtual void handleMessageWhenUp(cMessage *msg) override;
    void handleSelfMessage(cMessage *msg);

    // Life cycle of the protocol
    virtual void handleStartOperation(LifecycleOperation *operation) override {
        start();
    }
    virtual void handleStopOperation(LifecycleOperation *operation) override {
        stop();
    }
    virtual void handleCrashOperation(LifecycleOperation *operation) override {
        stop();
    }
    void start();
    void stop();

    // Notification when receive a signal
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details) override;

};

} /* namespace inet */

#endif
