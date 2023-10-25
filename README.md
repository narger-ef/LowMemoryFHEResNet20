# Low Memory FHE-based ResNet20
This repository contains a OpenFHE-based project that implements an encrypted version of the ResNet20 model, used to classify encrypted CIFAR10 images

### Architecture

<img src="img/architecture.png" alt="Architecture description" width=65%>

Bla bla

### How to run
In order to run the program OpenFHE needs to be installed in the system. Check [how to install OpenFHE](https://openfhe-development.readthedocs.io/en/latest/sphinx_rsts/intro/installation/installation.html).

Build the project using this command:
```
cmake --build "cmake-build-release" --target LowMemoryFHEResNet20
```
You can also use the `-j` flag in order to speed up the compilation, just put the number of cores of your machine. For instance:
```
cmake --build "cmake-build-release" --target LowMemoryFHEResNet20 -j 8
```
