
package inet.node.mlmorp;

import inet.node.inet.AdhocHost;
import inet.routing.mlmorp.Mlmorp;

//
// MLMORP Router
//

module MlmorpRouter extends AdhocHost
{
    submodules:
        mlmorp: Mlmorp {
            @display("p=825,226");
        }
    connections:
        mlmorp.ipOut --> tn.in++;
        mlmorp.ipIn <-- tn.out++;
}