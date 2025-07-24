// Author: AI Assistant - DNN Extension for MLMORP

#include "SimpleDNNModel.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>

namespace inet {

SimpleDNNModel::SimpleDNNModel(int inputSize, int hiddenSize1, int hiddenSize2, bool isClassification)
    : inputSize(inputSize), hiddenSize1(hiddenSize1), hiddenSize2(hiddenSize2), outputSize(1), isClassification(isClassification), learningRate(0.01)
{
    rng.seed(42);
    initializeWeights();
    initializeNormalization();
}

SimpleDNNModel::~SimpleDNNModel()
{
    // Destructor - vectors will be automatically cleaned up
}

void SimpleDNNModel::initializeWeights()
{
    double inputScale = sqrt(2.0 / inputSize);
    std::normal_distribution<double> dist(0.0, inputScale);
    weightsHidden1.resize(hiddenSize1);
    for (int i = 0; i < hiddenSize1; i++) {
        weightsHidden1[i].resize(inputSize);
        for (int j = 0; j < inputSize; j++) {
            weightsHidden1[i][j] = dist(rng);
        }
    }
    biasHidden1.resize(hiddenSize1, 0.0);
    double hidden1Scale = sqrt(2.0 / hiddenSize1);
    std::normal_distribution<double> distHidden1(0.0, hidden1Scale);
    weightsHidden2.resize(hiddenSize2);
    for (int i = 0; i < hiddenSize2; i++) {
        weightsHidden2[i].resize(hiddenSize1);
        for (int j = 0; j < hiddenSize1; j++) {
            weightsHidden2[i][j] = distHidden1(rng);
        }
    }
    biasHidden2.resize(hiddenSize2, 0.0);
    double hidden2Scale = sqrt(2.0 / hiddenSize2);
    std::normal_distribution<double> distHidden2(0.0, hidden2Scale);
    weightsOutput.resize(outputSize);
    for (int i = 0; i < outputSize; i++) {
        weightsOutput[i].resize(hiddenSize2);
        for (int j = 0; j < hiddenSize2; j++) {
            weightsOutput[i][j] = distHidden2(rng);
        }
    }
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
    std::vector<double> hiddenOutput1(hiddenSize1);
    for (int i = 0; i < hiddenSize1; i++) {
        double sum = biasHidden1[i];
        for (int j = 0; j < inputSize; j++) {
            sum += weightsHidden1[i][j] * input[j];
        }
        hiddenOutput1[i] = relu(sum);
    }
    std::vector<double> hiddenOutput2(hiddenSize2);
    for (int i = 0; i < hiddenSize2; i++) {
        double sum = biasHidden2[i];
        for (int j = 0; j < hiddenSize1; j++) {
            sum += weightsHidden2[i][j] * hiddenOutput1[j];
        }
        hiddenOutput2[i] = relu(sum);
    }
    double output = biasOutput[0];
    for (int j = 0; j < hiddenSize2; j++) {
        output += weightsOutput[0][j] * hiddenOutput2[j];
    }
    if (isClassification) {
        return sigmoid(output);
    } else {
        return output;
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

void SimpleDNNModel::setWeights(const std::vector<std::vector<double>>& weightsHidden1,
                   const std::vector<double>& biasHidden1,
                   const std::vector<std::vector<double>>& weightsHidden2,
                   const std::vector<double>& biasHidden2,
                   const std::vector<std::vector<double>>& weightsOutput,
                   const std::vector<double>& biasOutput) {
    this->weightsHidden1 = weightsHidden1;
    this->biasHidden1 = biasHidden1;
    this->weightsHidden2 = weightsHidden2;
    this->biasHidden2 = biasHidden2;
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

std::string SimpleDNNModel::getModelInfo() const {
    std::ostringstream oss;
    oss << "SimpleDNNModel architecture: (" << inputSize << "-" << hiddenSize1 << "-" << hiddenSize2 << "-" << outputSize << ")";
    return oss.str();
}

bool SimpleDNNModel::saveModel(const std::string& filename) const
{
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    file << "Architecture:" << std::endl;
    file << inputSize << " " << hiddenSize1 << " " << hiddenSize2 << " " << outputSize << " " << isClassification << std::endl;
    file << "Normalization:" << std::endl;
    for (int i = 0; i < inputSize; i++) {
        file << featureMeans[i] << " " << featureStds[i] << std::endl;
    }
    file << "Hidden1Weights:" << std::endl;
    for (int i = 0; i < hiddenSize1; i++) {
        for (int j = 0; j < inputSize; j++) {
            file << weightsHidden1[i][j] << " ";
        }
        file << std::endl;
    }
    file << "Hidden1Bias:" << std::endl;
    for (int i = 0; i < hiddenSize1; i++) {
        file << biasHidden1[i] << " ";
    }
    file << std::endl;
    file << "Hidden2Weights:" << std::endl;
    for (int i = 0; i < hiddenSize2; i++) {
        for (int j = 0; j < hiddenSize1; j++) {
            file << weightsHidden2[i][j] << " ";
        }
        file << std::endl;
    }
    file << "Hidden2Bias:" << std::endl;
    for (int i = 0; i < hiddenSize2; i++) {
        file << biasHidden2[i] << " ";
    }
    file << std::endl;
    file << "OutputWeights:" << std::endl;
    for (int i = 0; i < outputSize; i++) {
        for (int j = 0; j < hiddenSize2; j++) {
            file << weightsOutput[i][j] << " ";
        }
        file << std::endl;
    }
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
    int loadedInputSize, loadedHiddenSize1, loadedHiddenSize2, loadedOutputSize;
    bool loadedIsClassification;
    iss >> loadedInputSize >> loadedHiddenSize1 >> loadedHiddenSize2 >> loadedOutputSize >> loadedIsClassification;
    
    // Verify architecture matches
    if (loadedInputSize != inputSize || loadedHiddenSize1 != hiddenSize1 || 
        loadedHiddenSize2 != hiddenSize2 || loadedOutputSize != outputSize || loadedIsClassification != isClassification) {
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
    std::getline(file, line);  // "Hidden1Weights:"
    for (int i = 0; i < hiddenSize1; i++) {
        std::getline(file, line);
        std::istringstream iss(line);
        for (int j = 0; j < inputSize; j++) {
            iss >> weightsHidden1[i][j];
        }
    }
    
    // Load hidden layer bias
    std::getline(file, line);  // "Hidden1Bias:"
    std::getline(file, line);
    std::istringstream issBias1(line);
    for (int i = 0; i < hiddenSize1; i++) {
        issBias1 >> biasHidden1[i];
    }
    
    // Load hidden layer weights
    std::getline(file, line);  // "Hidden2Weights:"
    for (int i = 0; i < hiddenSize2; i++) {
        std::getline(file, line);
        std::istringstream iss(line);
        for (int j = 0; j < hiddenSize1; j++) {
            iss >> weightsHidden2[i][j];
        }
    }
    
    // Load hidden layer bias
    std::getline(file, line);  // "Hidden2Bias:"
    std::getline(file, line);
    std::istringstream issBias2(line);
    for (int i = 0; i < hiddenSize2; i++) {
        issBias2 >> biasHidden2[i];
    }
    
    // Load output layer weights
    std::getline(file, line);  // "OutputWeights:"
    for (int i = 0; i < outputSize; i++) {
        std::getline(file, line);
        std::istringstream iss(line);
        for (int j = 0; j < hiddenSize2; j++) {
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
