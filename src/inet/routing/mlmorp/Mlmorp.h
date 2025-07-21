// Author: Basheer Al-Qassab

#ifndef __INET_MLMORP_H
#define __INET_MLMORP_H

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <fstream>

// General INET includes
#include "inet/common/INETDefs.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/contract/IInterfaceTable.h"
#include "inet/networklayer/contract/ipv4/Ipv4Address.h"
#include "inet/networklayer/common/L3Address.h"
#include "inet/networklayer/ipv4/IIpv4RoutingTable.h"
#include "inet/networklayer/ipv4/Ipv4Header_m.h"
#include "inet/networklayer/ipv4/Ipv4InterfaceData.h"
#include "inet/networklayer/ipv4/Ipv4RoutingTable.h"
#include "inet/routing/base/RoutingProtocolBase.h"
#include "inet/networklayer/contract/INetfilter.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"
#include "inet/common/TimeTag_m.h"

// Include for mobility
#include "inet/common/geometry/common/Coord.h"
#include "inet/mobility/contract/IMobility.h"

// Include for Energy
#include "inet/power/contract/IEpEnergyStorage.h"

// Include for NodeStaus
#include "inet/common/lifecycle/NodeStatus.h"

// Internal includes
#include "inet/routing/mlmorp/Mlmorp_m.h"
#include "inet/routing/mlmorp/MlmorpRouteData.h"
#include "inet/routing/mlmorp/MlmorpNeighborTable.h"
#include "inet/routing/mlmorp/SimpleDNNModel.h"

namespace inet {

/**
 * MLMORP protocol implementation.
 */
class INET_API Mlmorp : public RoutingProtocolBase, public NetfilterBase::HookBase, public cListener
{

private:
    cMessage *event = nullptr;
    cMessage *purgeTimer = nullptr;   // A self message to use for purge event
    cPar *broadcastDelay = nullptr;

    unsigned int sequenceNumber = 0;
    simtime_t routeLifetime;
    simtime_t neighborLifetime;

    //Context
    cModule *host = nullptr;
    NetworkInterface *interface80211ptr = nullptr;
    int interfaceId = -1;
//    ModuleRefByPar<IRoutingTable> routingTable;
    opp_component_ptr<IMobility> mobility;
    opp_component_ptr<power::IEpEnergyStorage> energyStorage;
    opp_component_ptr<NodeStatus> nodeStatus;

    // Internal
    double alpha;
    double beta;
    double gamma;
    MlmorpNeighborTable neighborTable;

    // Packet delay tracking
    simtime_t currentPacketDelay;

    
    // DNN Model for routing decisions
    SimpleDNNModel* dnnModel;
    
    /**
     * Select the best next-hop neighbor using DNN model predictions
     * @param destination The destination address
     * @return The best neighbor address for routing
     */
    L3Address selectBestNeighborDNN(const L3Address& destination) const;

    int getCurrentBufferPacketNum() const;

protected:
    simtime_t beaconInterval;
    ModuleRefByPar<IInterfaceTable> ift;
    ModuleRefByPar<IIpv4RoutingTable> rt;
    ModuleRefByPar<INetfilter> networkProtocol;

public:
    Mlmorp();
    ~Mlmorp();

protected:
    // Initialization function
    virtual void initialize(int stage) override;

    // Operation of the function. Handling Messages
    virtual void handleMessageWhenUp(cMessage *msg) override;
    void handleSelfMessage(cMessage *msg);

    // Handling the purge event and performing the purge operation
    void reschedulePurgeTimer();
    void purge();

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

    // NetFilter
    virtual Result datagramPreRoutingHook(Packet *datagram) override;
    virtual Result datagramForwardHook(Packet *datagram) override { return ACCEPT; }
    virtual Result datagramPostRoutingHook(Packet *datagram) override { return ACCEPT; }
    virtual Result datagramLocalInHook(Packet *datagram) override { return ACCEPT; }
    virtual Result datagramLocalOutHook(Packet *datagram) override;

    // DNN routing
    Result routeDatagram(Packet *datagram);

    // Notification when receive a signal
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details) override;

};

} /* namespace inet */

#endif
