#!/usr/bin/env python3
"""
Complete Training Pipeline for MLMORP Reinforcement Learning

This script provides a complete pipeline for training and evaluating 
MLMORP reinforcement learning models. It includes:

1. Data preprocessing and validation
2. Initial DQN model training
3. Learning curve generation
4. Model comparison and analysis

Usage:
    python run_rl_training.py --data ../results/output.csv
"""

import os
import sys
import argparse
import subprocess
from pathlib import Path

def check_requirements():
    """Check if required packages are installed"""
    try:
        import numpy
        import pandas
        import tensorflow
        import matplotlib
        import seaborn
        import sklearn
        print("✓ All required packages are available")
        return True
    except ImportError as e:
        print(f"✗ Missing required package: {e}")
        print("Please install requirements: pip install -r requirements_rl.txt")
        return False

def validate_data_file(data_file):
    """Validate that the data file exists and has the expected format"""
    if not os.path.exists(data_file):
        print(f"✗ Data file not found: {data_file}")
        print("Please run your OMNeT++ simulation first to generate the data file.")
        return False
    
    try:
        import pandas as pd
        df = pd.read_csv(data_file, header=None, nrows=5)  # Just check first few rows
        print(f"✓ Data file validated: {len(df)} sample rows")
        return True
    except Exception as e:
        print(f"✗ Error reading data file: {e}")
        return False

def run_training_step(script_name, args, description):
    """Run a training step and handle errors"""
    print(f"\n{'='*60}")
    print(f"STEP: {description}")
    print('='*60)
    
    cmd = [sys.executable, script_name] + args
    print(f"Running: {' '.join(cmd)}")
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        print("✓ Step completed successfully")
        if result.stdout:
            print("Output:", result.stdout[-500:])  # Show last 500 chars
        return True
    except subprocess.CalledProcessError as e:
        print(f"✗ Step failed with error code {e.returncode}")
        if e.stdout:
            print("stdout:", e.stdout)
        if e.stderr:
            print("stderr:", e.stderr)
        return False
    except FileNotFoundError:
        print(f"✗ Script not found: {script_name}")
        return False

def main():
    parser = argparse.ArgumentParser(description='Complete MLMORP RL Training Pipeline')
    parser.add_argument('--data', '-d', required=True,
                       help='Path to simulation data file (output.csv)')
    parser.add_argument('--output-dir', '-o', default='trained_models',
                       help='Output directory for trained models')
    parser.add_argument('--skip-dnn', action='store_true',
                       help='Skip traditional DNN training')
    parser.add_argument('--skip-dqn', action='store_true',
                       help='Skip DQN training')
    parser.add_argument('--skip-plots', action='store_true',
                       help='Skip learning curve generation')
    parser.add_argument('--epochs', type=int, default=100,
                       help='Number of training epochs for DNN')
    parser.add_argument('--dqn-epochs', type=int, default=1000,
                       help='Number of training epochs for DQN')
    
    args = parser.parse_args()
    
    # Create output directory
    os.makedirs(args.output_dir, exist_ok=True)
    
    print("MLMORP Reinforcement Learning Training Pipeline")
    print("=" * 60)
    
    # Step 1: Check requirements
    print("\nStep 1: Checking requirements...")
    if not check_requirements():
        return 1
    
    # Step 2: Validate data
    print("\nStep 2: Validating data file...")
    if not validate_data_file(args.data):
        return 1
    
    success = True
    
    # Step 3: Train traditional DNN model (for comparison)
    if not args.skip_dnn:
        dnn_output = os.path.join(args.output_dir, "trained_dnn_model.txt")
        dnn_args = [
            '--input', args.data,
            '--output', dnn_output,
            '--epochs', str(args.epochs),
            '--no-plot'
        ]
        success &= run_training_step('train_mlmorp_dnn.py', dnn_args, 
                                   "Training traditional DNN model")
    
    # Step 4: Train DQN model
    if not args.skip_dqn and success:
        dqn_output = os.path.join(args.output_dir, "trained_dqn_model.txt")
        dqn_args = [
            '--input', args.data,
            '--output', dqn_output,
            '--epochs', str(args.dqn_epochs),
            '--no-plot'
        ]
        success &= run_training_step('train_dqn_model.py', dqn_args,
                                   "Training DQN model")
    
    # Step 5: Generate learning curves and analysis
    if not args.skip_plots and success:
        plot_args = [
            '--input', args.data,
            '--output-dir', args.output_dir
        ]
        success &= run_training_step('plot_learning_curves.py', plot_args,
                                   "Generating learning curves and analysis")
    
    # Final summary
    print(f"\n{'='*60}")
    print("TRAINING PIPELINE SUMMARY")
    print('='*60)
    
    if success:
        print("✓ All steps completed successfully!")
        print(f"\nTrained models saved in: {args.output_dir}")
        
        # List generated files
        print("\nGenerated files:")
        for file_path in Path(args.output_dir).iterdir():
            if file_path.is_file():
                print(f"  - {file_path.name}")
        
        print("\nNext steps:")
        print("1. Copy trained models to your simulation directory")
        print("2. Update your .ini file to use the trained models:")
        print(f"   *.host[*].routingTable.routingProtocol[*].dnnModelFile = \"{args.output_dir}/trained_dnn_model.txt\"")
        print(f"   *.host[*].routingTable.routingProtocol[*].dqnModelFile = \"{args.output_dir}/trained_dqn_model.txt\"")
        print("3. Run your simulation with different configurations")
        print("4. Compare performance using the generated learning curves")
        
    else:
        print("✗ Training pipeline failed!")
        print("Please check the error messages above and fix any issues.")
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())

