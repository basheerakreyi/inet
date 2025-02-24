
package inet.node.morpml;

import inet.node.inet.AdhocHost;
import inet.routing.morpml.Morpml;

//
// MORPML Router
//

module MorpmlRouter extends AdhocHost
{
    submodules:
        morpml: Morpml {
            @display("p=825,226");
        }
    connections:
        morpml.ipOut --> tn.in++;
        morpml.ipIn <-- tn.out++;
}