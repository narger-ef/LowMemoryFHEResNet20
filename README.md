# Low Memory FHE-based ResNet20
This repository contains a OpenFHE-based project that implements an encrypted version of the ResNet20 model, used to classify encrypted CIFAR10 images. The reference paper for this work is [Low memory overhead secure ResNet inference based on Fully Homomorphic Encryption](google.it), (2023, Lorenzo Rovida and Alberto Leporati).

The key idea behind this work is to propose a solution to run a CNN in relative small time (<5 mins on my Macbook M1 Pro) and, moreover, to use a small amount of RAM. Existing works use a lot of memory ([1]: $\approx$ 100GB, [2]: $\approx$ 500GB), while this implementation uses at most 16GB.

## Architecture

<img src="img/architecture.png" alt="Architecture description" width=75%>

Bla bla

## How to run
In order to run the program OpenFHE needs to be installed in the system. Check [how to install OpenFHE](https://openfhe-development.readthedocs.io/en/latest/sphinx_rsts/intro/installation/installation.html).

### 1) Build the project

Build the project using this command:
```
mkdir build
cmake --build "build" --target LowMemoryFHEResNet20
```
You can also use the `-j` flag in order to speed up the compilation, just put the number of cores of your machine. For instance:
```
cmake --build "build" --target LowMemoryFHEResNet20 -j 8
```

### 2) Creating the context

After building, go to the created `build` folder:
```
cd build
```
Now it is possible to run the program! But we need to define a couple of arguments before:

- `generate_keys`, a value in `[params_exp1, params_exp2, params_exp3, params_exp4]`
- `load_keys`, type: `string`
- `input`, type: `string`
  
The first execution should be launched with the `generate_keys` argument, using the preferred set of parameters. Check the paper to see the differences between them. For instance, we choose the set of parameters defined in the first experiment:
```
./LowMemoryFHEResNet20 generate_keys "params_exp1"
```
This command create the required keys and stores them in a new folder called `params_exp1`, in the root folder of the project.

### 3) Running the inference

Now we are able to launch an inference process using the just created set of keys and parameters. Simply use
```
./LowMemoryFHEResNet20 load_keys "params_exp1"
```
This command loads context and keys from the folder `params_exp1`, located in the root folder of the project, and performs an inference. Since no image is given as input, the default image is located in `inputs/louis.jpg`. We can, however, pass a custom image using the `input` argument, as follows:.
```
./LowMemoryFHEResNet20 load_keys "params_exp1" input "inputs/airplane4.png"
```
Notice that the image MUST be a 32x32x3 RGB image in `.jpg` or `.png` format. We use the MIT-licensed [stb](https://github.com/nothings/stb) library in order to read it.
Even for this argument, the starting position will be the root of the project.

## Interpreting the output
The output of the encrypted model is a vector consisting of 10 elements. In order to interpret it, it is enough to find the index of the maximum element. A sample output could be:

```
[7.2524807010, -2.6369680214, -1.0997904940,  6.0638060424, -4.0936126151, -0.5967846282, -2.1562481052, -1.0855666688, -0.9119194165, -0.7291559490 ]
```
In this case, the maximum value is at position 0. Just translate it using the following dictionary (from ResNet20 pretrained on CIFAR-10):

- 0: Airplane
- 1: Automobile
- 2: Bird
- 3: Cat
- 4: Deer
- 5: Dog
- 6: Frog
- 7: Horse
- 8: Ship
- 9: Truck

In the sample output, the classified image was the following:

<img src="inputs/airplane4.png" alt="Sample airplane input" width=8%>

So it was correct!

## Declaration

This is a proof of concept and, even though parameters are created wtih $\lambda = 128$ security bits, this circuit should not be used in production.

---

## Bibliography 
[1] D. Kim and C. Guyot, "Optimized Privacy-Preserving CNN Inference With Fully Homomorphic Encryption," in IEEE Transactions on Information Forensics and Security, vol. 18, pp. 2175-2187, 2023, doi: 10.1109/TIFS.2023.3263631.

[2] Lee, E., Lee, J. W., Lee, J., Kim, Y. S., Kim, Y., No, J. S., & Choi, W. (2022, June). Low-complexity deep convolutional neural networks on fully homomorphic encryption using multiplexed parallel convolutions. In International Conference on Machine Learning (pp. 12403-12422). PMLR.

[3] L. Rovida and A. Leporati, “Low memory overhead secure ResNet inference based on Fully Homomorphic Encryption,” International Journal of Neural Systems, vol. X, no. X, pp. XXX–XXX, 2023, doi: 10.XXXX/ijns.XXXX.XXXX.
