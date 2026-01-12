
package inet.node.rlmorp;

import inet.node.inet.AdhocHost;
import inet.routing.rlmorp.Rlmorp;

//
// RLMORP Router
//

module RlmorpRouter extends AdhocHost
{
    submodules:
        rlmorp: Rlmorp {
            @display("p=825,226");
        }
    connections:
        rlmorp.ipOut --> tn.in++;
        rlmorp.ipIn <-- tn.out++;
}