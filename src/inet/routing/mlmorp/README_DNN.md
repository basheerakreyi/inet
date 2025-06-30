# MLMORP with Deep Neural Network Extension

This document describes the Deep Neural Network (DNN) extension for the MLMORP routing protocol in OMNeT++.

## Overview

The DNN extension adds machine learning capabilities to the MLMORP routing protocol, allowing it to make routing decisions based on learned patterns from network features rather than just traditional metrics.

## Features

### Neural Network Architecture
- **Input Layer**: 6 neurons (configurable)
  - Residual Energy
  - Data Rate
  - Signal Power
  - Node Degree
  - SNIR (Signal-to-Noise-and-Interference Ratio)
  - Packet Delay
- **Hidden Layer**: 12 neurons (configurable)
  - ReLU activation function
- **Output Layer**: 1 neuron
  - Sigmoid activation (for classification)
  - Identity function (for regression)

### Key Features
- Pure C++ implementation (no external ML libraries)
- Configurable network architecture
- Support for pre-trained model loading
- Feature normalization
- Integration with existing MLMORP routing logic

## Usage

### 1. Enable DNN Routing

In your NED file or configuration, set the DNN parameters:

```ned
*.host[*].routingTable.routingProtocol[*].useDNNRouting = true
*.host[*].routingTable.routingProtocol[*].dnnInputSize = 6
*.host[*].routingTable.routingProtocol[*].dnnHiddenSize = 12
*.host[*].routingTable.routingProtocol[*].dnnClassification = true
```

### 2. Load Pre-trained Model (Optional)

```ned
*.host[*].routingTable.routingProtocol[*].dnnModelFile = "model_training/trained_model.txt"
```

### 3. Model File Format

The model file should contain:
- Architecture information
- Normalization parameters
- Weight matrices
- Bias vectors

Example model file structure:
```
Architecture:
6 12 1 1
Normalization:
0.5 0.3
1e7 5e6
1e-6 5e-7
5.0 3.0
10.0 5.0
0.01 0.005
HiddenWeights:
0.1 0.2 0.3 0.4 0.5 0.6
...
HiddenBias:
0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0
OutputWeights:
0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0 1.1 1.2
OutputBias:
0.0
```

## Implementation Details

### Files Added
- `SimpleDNNModel.h` - Header file for the DNN model class
- `SimpleDNNModel.cc` - Implementation of the DNN model

### Files Modified
- `Mlmorp.h` - Added DNN model integration
- `Mlmorp.cc` - Added DNN-based routing logic
- `Mlmorp.ned` - Added DNN configuration parameters
- `Makefile` - Added SimpleDNNModel compilation

### Training Files (in `model_training/` subfolder)
- `train_mlmorp_dnn.py` - Main training script
- `example_training.py` - Example script with sample data
- `requirements.txt` - Python dependencies
- `README_Training.md` - Training documentation
- `example_model.txt` - Example pre-trained model file

### Key Methods

#### SimpleDNNModel Class
- `predict()` - Make predictions using the neural network
- `selectBestNeighbor()` - Select best neighbor based on DNN scores
- `loadModel()` / `saveModel()` - Model persistence
- `setWeights()` / `setNormalization()` - Manual parameter setting

#### Mlmorp Class
- `selectBestNeighborDNN()` - DNN-based neighbor selection
- Integration in `handleMessageWhenUp()` for routing decisions

## Configuration Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `useDNNRouting` | bool | false | Enable DNN-based routing |
| `dnnInputSize` | int | 6 | Number of input features |
| `dnnHiddenSize` | int | 12 | Number of hidden layer neurons |
| `dnnClassification` | bool | true | Use classification (true) or regression (false) |
| `dnnModelFile` | string | "" | Path to pre-trained model file |

## Training the Model

The DNN model can be trained using the data collected by the existing data collection mechanism. The collected features in `results/output.csv` can be used to:

1. Calculate feature normalization parameters
2. Train the neural network weights
3. Save the trained model

### Training Process
1. Run simulations with data collection enabled
2. Process the collected data to extract features and labels
3. Train the neural network (using external tools like Python)
4. Save the trained weights and normalization parameters
5. Load the model in MLMORP using `dnnModelFile` parameter

### Quick Training Guide
```bash
# Navigate to training directory
cd model_training

# Install dependencies
pip install -r requirements.txt

# Train with real data
python train_mlmorp_dnn.py --input ../results/output.csv --output trained_model.txt

# Test with sample data
python example_training.py
```

For detailed training instructions, see `model_training/README_Training.md`.

## Performance Considerations

- The DNN model adds computational overhead to routing decisions
- Feature normalization is performed for each prediction
- Model loading happens once during initialization
- Memory usage scales with network size (hidden layer neurons)

## Future Enhancements

- Online learning capabilities
- More sophisticated feature engineering
- Multiple output neurons for different routing metrics
- Adaptive network architecture
- Integration with other ML algorithms

## Troubleshooting

### Common Issues
1. **Model loading fails**: Check file path and format
2. **Poor routing performance**: Verify feature normalization parameters
3. **Compilation errors**: Ensure all files are included in Makefile

### Debug Information
Enable detailed logging to see DNN predictions:
```
*.host[*].routingTable.routingProtocol[*].useDNNRouting = true
```

The protocol will output DNN scores and routing decisions in the simulation log.

## Example Configuration

```ned
network MlmorpNetwork
{
    parameters:
        *.host[*].routingTable.routingProtocol[*].typename = "Mlmorp"
        *.host[*].routingTable.routingProtocol[*].useDNNRouting = true
        *.host[*].routingTable.routingProtocol[*].dnnInputSize = 6
        *.host[*].routingTable.routingProtocol[*].dnnHiddenSize = 12
        *.host[*].routingTable.routingProtocol[*].dnnClassification = true
        *.host[*].routingTable.routingProtocol[*].dnnModelFile = "model_training/trained_model.txt"
}
```

This configuration enables DNN-based routing with a 6-12-1 neural network architecture using classification mode and loading a pre-trained model.

## File Structure

```
src/
├── Mlmorp.h                    # Main protocol header with DNN integration
├── Mlmorp.cc                   # Main protocol implementation with DNN routing
├── SimpleDNNModel.h            # DNN model header
├── SimpleDNNModel.cc           # DNN model implementation
├── Mlmorp.ned                  # Protocol configuration with DNN parameters
├── Makefile                    # Build configuration
├── README_DNN.md              # This documentation
└── model_training/            # Training scripts and tools
    ├── train_mlmorp_dnn.py    # Main training script
    ├── example_training.py    # Example training script
    ├── requirements.txt       # Python dependencies
    ├── README_Training.md     # Training documentation
    └── example_model.txt      # Example pre-trained model
``` 