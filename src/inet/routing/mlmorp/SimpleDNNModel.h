// Author: AI Assistant - DNN Extension for MLMORP

#ifndef __INET_SIMPLEDNNMODEL_H
#define __INET_SIMPLEDNNMODEL_H

#include <vector>
#include <cmath>
#include <random>
#include <algorithm>
#include <map>
#include <limits>

#include "inet/common/INETDefs.h"
#include "inet/networklayer/common/L3Address.h"
#include "inet/common/geometry/common/Coord.h"

namespace inet {

/**
 * Simple Deep Neural Network Model for MLMORP routing decisions.
 * 
 * This class implements a feedforward neural network with:
 * - 1 input layer (number of neurons = number of features)
 * - 1 hidden layer (configurable number of neurons)
 * - 1 output neuron (for binary classification or regression)
 * 
 * Features used:
 * - Residual Energy (normalized)
 * - Data Rate (normalized)
 * - Signal Power (normalized)
 * - Node Degree (normalized)
 * - SNIR (normalized)
 * - Packet Delay (normalized)
 */
class INET_API SimpleDNNModel
{
private:
    // Network architecture parameters
    int inputSize;           // Number of input features
    int hiddenSize;          // Number of neurons in hidden layer
    int outputSize;          // Number of output neurons (1 for binary classification)
    
    // Weight matrices and bias vectors
    std::vector<std::vector<double>> weightsHidden;    // Weights from input to hidden layer
    std::vector<double> biasHidden;                    // Bias for hidden layer
    std::vector<std::vector<double>> weightsOutput;    // Weights from hidden to output layer
    std::vector<double> biasOutput;                    // Bias for output layer
    
    // Feature normalization parameters
    std::vector<double> featureMeans;                  // Mean values for feature normalization
    std::vector<double> featureStds;                   // Standard deviations for feature normalization
    
    // Random number generator for weight initialization
    std::mt19937 rng;
    
    // Model configuration
    bool isClassification;                              // True for classification, false for regression
    double learningRate;                               // Learning rate for training (if implemented)
    
    /**
     * Initialize weights and biases using Xavier/Glorot initialization
     */
    void initializeWeights();
    
    /**
     * Initialize feature normalization parameters
     */
    void initializeNormalization();
    
    /**
     * ReLU activation function
     * @param x Input value
     * @return ReLU(x) = max(0, x)
     */
    double relu(double x) const;
    
    /**
     * Sigmoid activation function
     * @param x Input value
     * @return Sigmoid(x) = 1 / (1 + exp(-x))
     */
    double sigmoid(double x) const;
    
    /**
     * Forward pass through the network
     * @param input Input features (normalized)
     * @return Network output
     */
    double forwardPass(const std::vector<double>& input) const;
    
    /**
     * Normalize input features using pre-computed statistics
     * @param features Raw input features
     * @return Normalized features
     */
    std::vector<double> normalizeFeatures(const std::vector<double>& features) const;

public:
    /**
     * Constructor for SimpleDNNModel
     * @param inputSize Number of input features
     * @param hiddenSize Number of neurons in hidden layer
     * @param isClassification True for classification, false for regression
     */
    SimpleDNNModel(int inputSize = 6, int hiddenSize = 12, bool isClassification = true);
    
    /**
     * Destructor
     */
    ~SimpleDNNModel();
    
    /**
     * Predict routing decision based on neighbor features
     * @param residualEnergy Residual energy of the neighbor node
     * @param dataRate Data rate of the neighbor node
     * @param signalPower Signal power from the neighbor
     * @param nodeDegree Number of neighbors of the neighbor node
     * @param snir Signal-to-Noise-and-Interference Ratio
     * @param packetDelay Packet delay to the neighbor
     * @return Prediction score (higher is better for routing)
     */
    double predict(double residualEnergy, double dataRate, double signalPower, 
                   int nodeDegree, double snir, double packetDelay) const;
    
    /**
     * Predict routing decision using a feature vector
     * @param features Vector of features [residualEnergy, dataRate, signalPower, nodeDegree, snir, packetDelay]
     * @return Prediction score (higher is better for routing)
     */
    double predict(const std::vector<double>& features) const;
    
    /**
     * Get the best neighbor based on DNN predictions
     * @param neighbors Vector of neighbor addresses
     * @param neighborFeatures Map of neighbor addresses to their feature vectors
     * @return Best neighbor address for routing
     */
    L3Address selectBestNeighbor(const std::vector<L3Address>& neighbors,
                                   const std::map<L3Address, std::vector<double>>& neighborFeatures) const;
    
    /**
     * Set custom weights and biases (for loading pre-trained models)
     * @param weightsHidden Weights from input to hidden layer
     * @param biasHidden Bias for hidden layer
     * @param weightsOutput Weights from hidden to output layer
     * @param biasOutput Bias for output layer
     */
    void setWeights(const std::vector<std::vector<double>>& weightsHidden,
                   const std::vector<double>& biasHidden,
                   const std::vector<std::vector<double>>& weightsOutput,
                   const std::vector<double>& biasOutput);
    
    /**
     * Set feature normalization parameters
     * @param means Mean values for each feature
     * @param stds Standard deviations for each feature
     */
    void setNormalization(const std::vector<double>& means, const std::vector<double>& stds);
    
    /**
     * Get model architecture information
     * @return String describing the model architecture
     */
    std::string getModelInfo() const;
    
    /**
     * Save model weights to file (for persistence)
     * @param filename Output file name
     * @return True if successful, false otherwise
     */
    bool saveModel(const std::string& filename) const;
    
    /**
     * Load model weights from file
     * @param filename Input file name
     * @return True if successful, false otherwise
     */
    bool loadModel(const std::string& filename);
};

} // namespace inet

#endif // __INET_SIMPLEDNNMODEL_H 