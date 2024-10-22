
package inet.node.mmlrp;

import inet.node.inet.AdhocHost;
import inet.routing.mmlrp.Mmlrp;

//
// MMLRP Router
//

module MmlrpRouter extends AdhocHost
{
    submodules:
        mmlrp: Mmlrp {
            @display("p=825,226");
        }
    connections:
        mmlrp.ipOut --> tn.in++;
        mmlrp.ipIn <-- tn.out++;
}