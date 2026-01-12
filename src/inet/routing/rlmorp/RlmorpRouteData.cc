
// Author: Basheer Al-Qassab

#include "inet/routing/rlmorp/RlmorpRouteData.h"

namespace inet {

// This function is added to show the content of the route entry
// related to RLMORP in routing table.
std::string RlmorpRouteData::str() const
{
    std::stringstream out;
    out << Ipv4Route::str();
    out << " Cost:" << routeCost;
    out << " SeqNo:" << sequenceNumber;
    out << " ExpirTime:" << expirTime;
    return out.str();
}

} /* namespace inet */

