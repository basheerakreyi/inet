// Author: AI Assistant - DNN Extension for MLMORP

#include "SimpleDNNModel.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>

namespace inet {

SimpleDNNModel::SimpleDNNModel(int inputSize, int hiddenSize, bool isClassification)
    : inputSize(inputSize), hiddenSize(hiddenSize), outputSize(1), isClassification(isClassification), learningRate(0.01)
{
    // Initialize random number generator with a seed
    rng.seed(42);
    
    // Initialize weights and biases
    initializeWeights();
    
    // Initialize feature normalization parameters
    initializeNormalization();
}

SimpleDNNModel::~SimpleDNNModel()
{
    // Destructor - vectors will be automatically cleaned up
}

void SimpleDNNModel::initializeWeights()
{
    // Initialize weights from input to hidden layer using Xavier/Glorot initialization
    double inputScale = sqrt(2.0 / inputSize);
    std::normal_distribution<double> dist(0.0, inputScale);
    
    weightsHidden.resize(hiddenSize);
    for (int i = 0; i < hiddenSize; i++) {
        weightsHidden[i].resize(inputSize);
        for (int j = 0; j < inputSize; j++) {
            weightsHidden[i][j] = dist(rng);
        }
    }
    
    // Initialize bias for hidden layer
    biasHidden.resize(hiddenSize, 0.0);
    
    // Initialize weights from hidden to output layer
    double hiddenScale = sqrt(2.0 / hiddenSize);
    std::normal_distribution<double> distHidden(0.0, hiddenScale);
    
    weightsOutput.resize(outputSize);
    for (int i = 0; i < outputSize; i++) {
        weightsOutput[i].resize(hiddenSize);
        for (int j = 0; j < hiddenSize; j++) {
            weightsOutput[i][j] = distHidden(rng);
        }
    }
    
    // Initialize bias for output layer
    biasOutput.resize(outputSize, 0.0);
}

void SimpleDNNModel::initializeNormalization()
{
    // Initialize feature normalization parameters with reasonable defaults
    // These values should be updated based on actual data statistics
    
    featureMeans.resize(inputSize);
    featureStds.resize(inputSize);
    
    // Default normalization parameters (should be tuned based on actual data)
    // [residualEnergy, dataRate, signalPower, nodeDegree, buffPktNo]
    featureMeans = {0.5, 1e7, 1e-6, 5.0, 10.0};  // Mean values
    featureStds = {0.3, 5e6, 5e-7, 3.0, 5.0};   // Standard deviations
    
    // Ensure no division by zero
    for (int i = 0; i < inputSize; i++) {
        if (featureStds[i] < 1e-10) {
            featureStds[i] = 1.0;
        }
    }
}

double SimpleDNNModel::relu(double x) const
{
    return std::max(0.0, x);
}

double SimpleDNNModel::sigmoid(double x) const
{
    // Avoid overflow for large negative values
    if (x < -500) return 0.0;
    if (x > 500) return 1.0;
    return 1.0 / (1.0 + exp(-x));
}

std::vector<double> SimpleDNNModel::normalizeFeatures(const std::vector<double>& features) const
{
    std::vector<double> normalized(inputSize);
    
    for (int i = 0; i < inputSize && i < features.size(); i++) {
        normalized[i] = (features[i] - featureMeans[i]) / featureStds[i];
    }
    
    return normalized;
}

double SimpleDNNModel::forwardPass(const std::vector<double>& input) const
{
    // Forward pass through hidden layer
    std::vector<double> hiddenOutput(hiddenSize);
    
    for (int i = 0; i < hiddenSize; i++) {
        double sum = biasHidden[i];
        for (int j = 0; j < inputSize; j++) {
            sum += weightsHidden[i][j] * input[j];
        }
        hiddenOutput[i] = relu(sum);
    }
    
    // Forward pass through output layer
    double output = biasOutput[0];
    for (int j = 0; j < hiddenSize; j++) {
        output += weightsOutput[0][j] * hiddenOutput[j];
    }
    
    // Apply activation function based on task type
    if (isClassification) {
        return sigmoid(output);
    } else {
        return output;  // Identity function for regression
    }
}

double SimpleDNNModel::predict(double residualEnergy, double dataRate, double signalPower, 
                              int nodeDegree, double buffPktNo) const
{
    // Create feature vector
    std::vector<double> features = {residualEnergy, dataRate, signalPower, 
                                   static_cast<double>(nodeDegree), buffPktNo};
    
    return predict(features);
}

double SimpleDNNModel::predict(const std::vector<double>& features) const
{
    // Normalize features
    std::vector<double> normalizedFeatures = normalizeFeatures(features);
    
    // Forward pass
    return forwardPass(normalizedFeatures);
}

L3Address SimpleDNNModel::selectBestNeighbor(const std::vector<L3Address>& neighbors,
                                            const std::map<L3Address, std::vector<double>>& neighborFeatures) const
{
    if (neighbors.empty()) {
        return L3Address();
    }
    
    L3Address bestNeighbor = neighbors[0];
    double bestScore = -std::numeric_limits<double>::max();
    
    for (const auto& neighbor : neighbors) {
        auto it = neighborFeatures.find(neighbor);
        if (it != neighborFeatures.end()) {
            double score = predict(it->second);
            if (score > bestScore) {
                bestScore = score;
                bestNeighbor = neighbor;
            }
        }
    }
    
    return bestNeighbor;
}

void SimpleDNNModel::setWeights(const std::vector<std::vector<double>>& weightsHidden,
                               const std::vector<double>& biasHidden,
                               const std::vector<std::vector<double>>& weightsOutput,
                               const std::vector<double>& biasOutput)
{
    this->weightsHidden = weightsHidden;
    this->biasHidden = biasHidden;
    this->weightsOutput = weightsOutput;
    this->biasOutput = biasOutput;
}

void SimpleDNNModel::setNormalization(const std::vector<double>& means, const std::vector<double>& stds)
{
    this->featureMeans = means;
    this->featureStds = stds;
    
    // Ensure no division by zero
    for (int i = 0; i < featureStds.size(); i++) {
        if (featureStds[i] < 1e-10) {
            featureStds[i] = 1.0;
        }
    }
}

std::string SimpleDNNModel::getModelInfo() const
{
    std::ostringstream oss;
    oss << "SimpleDNNModel - Architecture: " << inputSize << "->" << hiddenSize << "->" << outputSize;
    oss << " (Task: " << (isClassification ? "Classification" : "Regression") << ")";
    return oss.str();
}

bool SimpleDNNModel::saveModel(const std::string& filename) const
{
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    // Save model architecture
    file << "Architecture:" << std::endl;
    file << inputSize << " " << hiddenSize << " " << outputSize << " " << isClassification << std::endl;
    
    // Save normalization parameters
    file << "Normalization:" << std::endl;
    for (int i = 0; i < inputSize; i++) {
        file << featureMeans[i] << " " << featureStds[i] << std::endl;
    }
    
    // Save hidden layer weights
    file << "HiddenWeights:" << std::endl;
    for (int i = 0; i < hiddenSize; i++) {
        for (int j = 0; j < inputSize; j++) {
            file << weightsHidden[i][j] << " ";
        }
        file << std::endl;
    }
    
    // Save hidden layer bias
    file << "HiddenBias:" << std::endl;
    for (int i = 0; i < hiddenSize; i++) {
        file << biasHidden[i] << " ";
    }
    file << std::endl;
    
    // Save output layer weights
    file << "OutputWeights:" << std::endl;
    for (int i = 0; i < outputSize; i++) {
        for (int j = 0; j < hiddenSize; j++) {
            file << weightsOutput[i][j] << " ";
        }
        file << std::endl;
    }
    
    // Save output layer bias
    file << "OutputBias:" << std::endl;
    for (int i = 0; i < outputSize; i++) {
        file << biasOutput[i] << " ";
    }
    file << std::endl;
    
    file.close();
    return true;
}

bool SimpleDNNModel::loadModel(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    
    // Load architecture
    std::getline(file, line);  // "Architecture:"
    std::getline(file, line);
    std::istringstream iss(line);
    int loadedInputSize, loadedHiddenSize, loadedOutputSize;
    bool loadedIsClassification;
    iss >> loadedInputSize >> loadedHiddenSize >> loadedOutputSize >> loadedIsClassification;
    
    // Verify architecture matches
    if (loadedInputSize != inputSize || loadedHiddenSize != hiddenSize || 
        loadedOutputSize != outputSize || loadedIsClassification != isClassification) {
        file.close();
        return false;
    }
    
    // Load normalization parameters
    std::getline(file, line);  // "Normalization:"
    for (int i = 0; i < inputSize; i++) {
        std::getline(file, line);
        std::istringstream iss(line);
        iss >> featureMeans[i] >> featureStds[i];
    }
    
    // Load hidden layer weights
    std::getline(file, line);  // "HiddenWeights:"
    for (int i = 0; i < hiddenSize; i++) {
        std::getline(file, line);
        std::istringstream iss(line);
        for (int j = 0; j < inputSize; j++) {
            iss >> weightsHidden[i][j];
        }
    }
    
    // Load hidden layer bias
    std::getline(file, line);  // "HiddenBias:"
    std::getline(file, line);
    std::istringstream issBias(line);
    for (int i = 0; i < hiddenSize; i++) {
        issBias >> biasHidden[i];
    }
    
    // Load output layer weights
    std::getline(file, line);  // "OutputWeights:"
    for (int i = 0; i < outputSize; i++) {
        std::getline(file, line);
        std::istringstream iss(line);
        for (int j = 0; j < hiddenSize; j++) {
            iss >> weightsOutput[i][j];
        }
    }
    
    // Load output layer bias
    std::getline(file, line);  // "OutputBias:"
    std::getline(file, line);
    std::istringstream issOutputBias(line);
    for (int i = 0; i < outputSize; i++) {
        issOutputBias >> biasOutput[i];
    }
    
    file.close();
    return true;
}

} // namespace inet 
