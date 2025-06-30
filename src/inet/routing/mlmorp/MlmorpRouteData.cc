
// Author: Basheer Al-Qassab

#include "inet/routing/mlmorp/MlmorpRouteData.h"

namespace inet {

// This function is added to show the content of the route entry
// related to MLMORP in routing table.
std::string MlmorpRouteData::str() const
{
    std::stringstream out;
    out << Ipv4Route::str();
    out << " Cost:" << routeCost;
    out << " SeqNo:" << sequenceNumber;
    out << " ExpirTime:" << expirTime;
    return out.str();
}

} /* namespace inet */
