//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


package inet.node.mygpsr;

import inet.node.inet.AdhocHost;
import inet.routing.mygpsr.MyGpsr;

module MyGpsrRouter extends AdhocHost
{
    submodules:
        mygpsr: MyGpsr {
            parameters:
                @display("p=825,226");
        }
    connections:
        mygpsr.ipOut --> tn.in++;
        mygpsr.ipIn <-- tn.out++;
}

