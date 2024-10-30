
package inet.node.morp;

import inet.node.inet.AdhocHost;
import inet.routing.morp.Morp;

//
// MORP Router
//

module MorpRouter extends AdhocHost
{
    submodules:
        morp: Morp {
            @display("p=825,226");
        }
    connections:
        morp.ipOut --> tn.in++;
        morp.ipIn <-- tn.out++;
}