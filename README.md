# MORP & MORPML – Multi-Objective Routing Protocol for OMNeT++/INET

## Overview

This repository provides an extension to the INET Framework for OMNeT++, introducing a novel **Multi-Objective Routing Protocol (MORP)**, as well as an enhanced version, **MORPML**, which integrates machine learning techniques into the routing decision process.

MORP is designed to optimize routing decisions in Mobile Ad hoc Networks (MANETs) by considering multiple cross-layer metrics, including:
- **Residual energy**
- **Data rate**
- **Hop count**
- **Node degree**
- **Bandwidth cost**

These metrics are used to build a composite cost function, allowing the routing protocol to make more intelligent and energy-efficient decisions compared to traditional single-metric protocols such as DSDV or AODV.

## MORPML – Machine Learning Integration

MORPML builds upon the MORP protocol by incorporating supervised machine learning models to predict optimal next-hop decisions. A key objective of MORPML is to further enhance routing performance in heterogeneous MANET environments by learning from:
- Real-time network conditions (e.g., energy levels, delay, congestion)
- Historical performance data
- Simulated datasets generated during routing operations

Data collection modules are implemented to log routing and node metrics into CSV files during simulation. These logs can be used to train ML models externally (e.g., using Python), which are later integrated into the OMNeT++ simulation using C++ inference wrappers.

## Key Features

- Seamless integration with the INET Framework.
- Lightweight logging infrastructure for machine learning data collection.
- Extensible cost function supporting custom weights and metrics.
- Sequence number support to ensure loop-freeness and up-to-date routing.
- Modifiable through `.ini` configuration for easy experimentation.

## Usage

You can run the simulation using standard `.ini` configuration files and extend the parameters to control:
- Cost function weights
- Logging frequency
- ML model behavior (for MORPML)

## License

This project is **licensed under the GNU Lesser General Public License (LGPL) version 3**, in compliance with the [INET Framework](https://github.com/inet-framework/inet).

