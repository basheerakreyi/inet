
// Author: Basheer Al-Qassab

#include "inet/routing/morpml/MorpmlRouteData.h"

namespace inet {

// This function is added to show the content of the route entry
// related to MORPML in routing table.
std::string MorpmlRouteData::str() const
{
    std::stringstream out;
    out << Ipv4Route::str();
    out << " Cost:" << routeCost;
    out << " SeqNo:" << sequenceNumber;
    out << " ExpirTime:" << expirTime;
    return out.str();
}

} /* namespace inet */
