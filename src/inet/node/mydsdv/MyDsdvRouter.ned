
package inet.node.mydsdv;

import inet.node.inet.AdhocHost;
import inet.routing.mydsdv.MyDsdv;

//
// A DSDV router.
//
module MyDsdvRouter extends AdhocHost
{
    submodules:
        mydsdv: MyDsdv {
            @display("p=825,226");
        }
    connections:
        mydsdv.ipOut --> tn.in++;
        mydsdv.ipIn <-- tn.out++;
}

