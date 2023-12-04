#!/bin/bash

# Your command here
your_command() {
    echo "Running command with argument: $1"
    # Replace the line above with your actual command
}

# Define an array of arguments


arguments=(#"inputs/problematic/0_0_3.png"
            #"inputs/problematic/1_0_0.png"
            #"inputs/problematic/2_0_5.png"
            #"inputs/problematic/3_0_0.png"
            #"inputs/problematic/4_0_5.png"
            #"inputs/problematic/5_0_8.png"
            #"inputs/problematic/6_0_0.png"
            #"inputs/problematic/7_1_3.png"
            #"inputs/problematic/8_1_0.png"
            #"inputs/problematic/9_1_3.png"
            #"inputs/problematic/10_1_5.png"
            #"inputs/problematic/11_1_3.png"
            #"inputs/problematic/12_1_6.png"
            #"inputs/problematic/13_1_8.png"
            #"inputs/problematic/14_1_4.png"
            #"inputs/problematic/15_1_5.png"
            #"inputs/problematic/16_2_2.png"
            #"inputs/problematic/17_2_5.png"
            #"inputs/problematic/18_2_3.png"
            #"inputs/problematic/19_2_5.png"
            #"inputs/problematic/20_2_8.png"
            #"inputs/problematic/21_2_0.png"
            #"inputs/problematic/22_2_0.png"
            #"inputs/problematic/23_3_3.png"
            #"inputs/problematic/24_3_5.png"
            #"inputs/problematic/25_3_3.png"
            #"inputs/problematic/26_3_3.png"
            #"inputs/problematic/27_4_4.png"
            #"inputs/problematic/28_4_5.png"
            #"inputs/problematic/29_4_3.png"
            #"inputs/problematic/30_5_5.png"
            #"inputs/problematic/31_5_3.png"
            #"inputs/problematic/32_5_5.png"
            #"inputs/problematic/33_5_5.png"
            #"inputs/problematic/34_6_3.png"
            #"inputs/problematic/35_6_3.png"
            #"inputs/problematic/36_6_5.png"
            #"inputs/problematic/37_6_5.png"
            "inputs/problematic/38_7_5.png"
            "inputs/problematic/39_7_0.png"
            "inputs/problematic/40_7_5.png"
            "inputs/problematic/41_7_3.png"
            "inputs/problematic/42_8_5.png"
            "inputs/problematic/43_8_5.png"
            "inputs/problematic/44_8_5.png"
            "inputs/problematic/45_9_5.png"
            "inputs/problematic/46_9_3.png"
            "inputs/problematic/47_9_3.png"
            "inputs/problematic/48_9_5.png"
            "inputs/problematic/49_9_8.png"
            "inputs/problematic/50_9_3.png"
            "inputs/problematic/51_9_4.png"
            "inputs/problematic/52_9_5.png")

# Run the command for each argument in the array
for arg in "${arguments[@]}"; do
    ./LowMemoryFHEResNet20 load_keys "keys_exp3" input "$arg" | tee -a output_exp3.txt
    echo "*******"
done