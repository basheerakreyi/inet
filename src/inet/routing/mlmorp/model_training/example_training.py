#!/usr/bin/env python3
"""
Example script to demonstrate MLMORP DNN training

This script creates sample data and trains a DNN model to show how the training process works.
"""

import pandas as pd
import numpy as np
import os
from train_mlmorp_dnn import MLMORPDNNTrainer

def create_sample_data(output_file='sample_data.csv', num_samples=1000):
    """
    Create sample data for demonstration purposes
    
    Args:
        output_file: Output CSV file path
        num_samples: Number of sample data points to generate
    """
    print(f"Creating sample data with {num_samples} samples...")
    
    # Create sample data
    np.random.seed(42)
    
    data = {
        'simTime': np.random.uniform(0, 100, num_samples),
        'treeId': np.random.randint(1, 1000, num_samples),
        'sourceAddress': [f"192.168.1.{np.random.randint(1, 255)}" for _ in range(num_samples)],
        'destAddress': [f"192.168.1.{np.random.randint(1, 255)}" for _ in range(num_samples)],
        'srcMacAddress': [f"AA:BB:CC:DD:EE:{np.random.randint(10, 99):02d}" for _ in range(num_samples)],
        'destMacAddress': [f"AA:BB:CC:DD:EE:{np.random.randint(10, 99):02d}" for _ in range(num_samples)],
        'nodeDegree': np.random.randint(1, 20, num_samples),
        'residualEnergy': np.random.uniform(0.1, 1.0, num_samples),
        'dataRate': np.random.uniform(1e6, 1e8, num_samples),
        'signalPower': np.random.uniform(1e-7, 1e-5, num_samples),
        'buffPktNo': np.random.uniform(5.0, 20.0, num_samples),
    }
    
    # Create DataFrame
    df = pd.DataFrame(data)
    
    # Save to CSV (without header to match OMNeT++ format)
    df.to_csv(output_file, index=False, header=False)
    
    print(f"Sample data saved to {output_file}")
    return output_file

def main():
    """Main function to demonstrate the training process"""
    
    # Create sample data
    sample_file = create_sample_data()
    
    # Create trainer
    trainer = MLMORPDNNTrainer(
        input_size=5,
        hidden_size=12,
        learning_rate=0.001
    )
    
    # Load and preprocess data
    print("\n" + "="*50)
    print("LOADING AND PREPROCESSING DATA")
    print("="*50)
    
    X, y = trainer.load_data(sample_file)
    if X is None:
        return
    
    X_train, X_test, y_train, y_test = trainer.preprocess_data(X, y)
    
    # Build and train model
    print("\n" + "="*50)
    print("BUILDING AND TRAINING MODEL")
    print("="*50)
    
    model = trainer.build_model()
    history = trainer.train_model(X_train, y_train, X_test, y_test, 
                                 epochs=50, batch_size=32)  # Reduced epochs for demo
    
    # Evaluate model
    print("\n" + "="*50)
    print("EVALUATING MODEL")
    print("="*50)
    
    accuracy, predictions = trainer.evaluate_model(X_test, y_test)
    
    # Save model
    print("\n" + "="*50)
    print("SAVING MODEL")
    print("="*50)
    
    trainer.save_model('example_trained_model.txt')
    
    # Plot training history
    print("\n" + "="*50)
    print("PLOTTING TRAINING HISTORY")
    print("="*50)
    
    trainer.plot_training_history(history)
    
    print("\n" + "="*50)
    print("TRAINING COMPLETED SUCCESSFULLY!")
    print("="*50)
    print(f"Sample data: {sample_file}")
    print(f"Trained model: example_trained_model.txt")
    print(f"Test accuracy: {accuracy:.4f}")
    print(f"Model JSON: example_trained_model.json")
    print(f"Training plot: training_history.png")
    print("\nTo use this model in OMNeT++:")
    print("1. Copy example_trained_model.txt to your simulation directory")
    print("2. Configure your NED file:")
    print("   *.host[*].routingTable.routingProtocol[*].useDNNRouting = true")
    print("   *.host[*].routingTable.routingProtocol[*].dnnModelFile = \"example_trained_model.txt\"")
    
    # Clean up sample data
    if os.path.exists(sample_file):
        os.remove(sample_file)
        print(f"Cleaned up sample data file: {sample_file}")

if __name__ == "__main__":
    main() 