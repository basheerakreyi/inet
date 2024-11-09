
// Author: Basheer Al-Qassab

#ifndef __INET_MORPROUTEDATA_H
#define __INET_MORPROUTEDATA_H

#include "inet/networklayer/ipv4/IIpv4RoutingTable.h"

namespace inet {

class INET_API MorpRouteData : public Ipv4Route
{
  protected:
    unsigned int sequenceNumber; // Originated from destination to ensure loop freeness.
    simtime_t expirTime;         // Time the routing entry is valid until
    float routeCost;             // Cost of the route (The cost of the route can be based on multi-metrics)

  public:
    // This function will check if the route is valid only if expirTime is set to zero or the expirTime is more than current simulation time
    virtual bool isValid() const override { return expirTime == 0 || expirTime > simTime(); }
    bool isExpired() const { return expirTime != 0 && expirTime <= simTime(); }

    simtime_t getExpirTime() const { return expirTime; }
    void setExpirTime(simtime_t time) { expirTime = time; }

    unsigned int getSequenceNumber() const { return sequenceNumber; }
    void setSequenceNumber(int i) { sequenceNumber = i; }

    float getRouteCost() const { return routeCost; }
    void setRouteCost(float i) { routeCost = i; }

    virtual std::string str() const override;

};

} /* namespace inet */

#endif
