
#ifndef __INET_MYDSDV_H
#define __INET_MYDSDV_H

#include <stdio.h>
#include <string.h>

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
#include "inet/routing/mydsdv/MyDsdvHello_m.h"

namespace inet {

/**
 * DSDV protocol implementation.
 */
class INET_API MyDsdv : public RoutingProtocolBase
{
  private:
    struct ForwardEntry {
        cMessage *event = nullptr;
        Packet *hello = nullptr;

        ForwardEntry() {}
        ~ForwardEntry();
    };

    bool isForwardHello = false;
    cMessage *event = nullptr;
    cPar *broadcastDelay = nullptr;
    std::list<ForwardEntry *> *forwardList = nullptr;
    NetworkInterface *interface80211ptr = nullptr;
    int interfaceId = -1;
    unsigned int sequencenumber = 0;
    simtime_t routeLifetime;
    cModule *host = nullptr;

  protected:
    simtime_t helloInterval;
    ModuleRefByPar<IInterfaceTable> ift;
    ModuleRefByPar<IIpv4RoutingTable> rt;

  public:
    MyDsdv();
    ~MyDsdv();

  protected:
//    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage *msg) override;

    void handleSelfMessage(cMessage *msg);

    // lifecycle
    virtual void handleStartOperation(LifecycleOperation *operation) override { start(); }
    virtual void handleStopOperation(LifecycleOperation *operation) override { stop(); }
    virtual void handleCrashOperation(LifecycleOperation *operation) override { stop(); }
    void start();
    void stop();
};

/**
 * IPv4 route used by the DSDV protocol (DSDV module).
 */
class INET_API MyDsdvIpv4Route : public Ipv4Route
{
  protected:
    unsigned int sequencenumber; // originated from destination. Ensures loop freeness.
    simtime_t expiryTime; // time the routing entry is valid until

  public:
    virtual bool isValid() const override { return expiryTime == 0 || expiryTime > simTime(); }

    simtime_t getExpiryTime() const { return expiryTime; }
    void setExpiryTime(simtime_t time) { expiryTime = time; }
    void setSequencenumber(int i) { sequencenumber = i; }
    unsigned int getSequencenumber() const { return sequencenumber; }
};

} // namespace inet

#endif

