
#ifndef __INET_MMLRP_H
#define __INET_MMLRP_H

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <fstream>

#include <list>
#include <map>
#include <vector>

#include "inet/common/packet/Packet.h"
#include "inet/networklayer/contract/IInterfaceTable.h"
#include "inet/networklayer/contract/ipv4/Ipv4Address.h"
#include "inet/networklayer/ipv4/IIpv4RoutingTable.h"
#include "inet/networklayer/ipv4/Ipv4Header_m.h"
#include "inet/networklayer/ipv4/Ipv4InterfaceData.h"
#include "inet/networklayer/ipv4/Ipv4RoutingTable.h"
#include "inet/routing/base/RoutingProtocolBase.h"

#include "inet/routing/mmlrp/Mmlrp_m.h"
#include "inet/routing/mmlrp/MmlrpNeighborTable.h"

#include "inet/networklayer/contract/INetfilter.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"
#include "inet/power/contract/IEpEnergyStorage.h"

namespace inet {

/**
 * MMLRP protocol implementation.
 */
class INET_API Mmlrp : public RoutingProtocolBase, public NetfilterBase::HookBase, public cListener
{
private:
    cMessage *event = nullptr;
    cPar *broadcastDelay = nullptr;

    unsigned int sequencenumber = 0;
    simtime_t routeLifetime;

    //Context
    cModule *host = nullptr;
    NetworkInterface *interface80211ptr = nullptr;
    int interfaceId = -1;
    opp_component_ptr<power::IEpEnergyStorage> energyStorage;

    // Internal
    MmlrpNeighborTable neighborTable;

  protected:
    simtime_t beaconInterval;
    ModuleRefByPar<IInterfaceTable> ift;
    ModuleRefByPar<IIpv4RoutingTable> rt;
    ModuleRefByPar<INetfilter> networkProtocol;

  public:
    Mmlrp();
    ~Mmlrp();

  protected:
    // Initialization function
    virtual void initialize(int stage) override;

    // Operation of the function. Handling Messages
    virtual void handleMessageWhenUp(cMessage *msg) override;
    void handleSelfMessage(cMessage *msg);

    // Life cycle of the protocol
    virtual void handleStartOperation(LifecycleOperation *operation) override { start(); }
    virtual void handleStopOperation(LifecycleOperation *operation) override { stop(); }
    virtual void handleCrashOperation(LifecycleOperation *operation) override { stop(); }
    void start();
    void stop();

    // netfilter
    virtual Result datagramPreRoutingHook(Packet *datagram) override;
    virtual Result datagramForwardHook(Packet *datagram) override { return ACCEPT; }
    virtual Result datagramPostRoutingHook(Packet *datagram) override { return ACCEPT; }
    virtual Result datagramLocalInHook(Packet *datagram) override { return ACCEPT; }
    virtual Result datagramLocalOutHook(Packet *datagram) override{ return ACCEPT; }

    // notification
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details) override;

};

/**
 * IPv4 route used by the MMLRP protocol (MMLRP module).
 */
class INET_API MmlrpIpv4Route : public Ipv4Route
{
  protected:
    unsigned int sequencenumber; // originated from destination. Ensures loop freeness.
    simtime_t expiryTime;        // time the routing entry is valid until

  public:
    // This function will check if the route is valid only if expiryTime is set to zero or the expiryTime is more than current simulation time
    virtual bool isValid() const override { return expiryTime == 0 || expiryTime > simTime(); }

    simtime_t getExpiryTime() const { return expiryTime; }
    void setExpiryTime(simtime_t time) { expiryTime = time; }

    unsigned int getSequencenumber() const { return sequencenumber; }
    void setSequencenumber(int i) { sequencenumber = i; }

};

}

#endif
