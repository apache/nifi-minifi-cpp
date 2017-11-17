# Building the MiNiFi - C++ TensorFlow Extension

The TensorFlow extension depends on the libtensorflow_cc.so (C++) library.
In order to build the extension, CMake must be able to locate the TensorFlow
headers as well as the built libtensorflow_cc.so library. Additionally, the
system must be able to locate the library in its runtime library search path
in order for MiNiFi to run.

## CentOS 7

This extension is known to work on CentOS 7 with cuDNN 6, CUDA 8, and TensorFlow 1.4.

1. If using CUDA, first install NVIDIA drivers for the system's hardware
2. If using CUDA, install CUDA 8 using the official NVIDIA repositories

```bash
sudo yum install cuda-repo-rhel7-9.0.176-1.x86_64.rpm
sudo yum install cuda-{core,command-line-tools,curand-dev,cufft-dev,cublas-dev,cusolver-dev}-8-0
```

3. If using CUDA, install cuDNN to /usr/local/cuda-8.0

```bash
tar xvf cudnn-8.0-linux-x64-v6.0.tgz
cd cuda
sudo cp lib64/libcudnn.so.6.0.21 /usr/local/cuda-8.0/lib64/
sudo ln -s /usr/local/cuda-8.0/lib64/libcudnn.so{.6.0.21,.6}
sudo ln -s /usr/local/cuda-8.0/lib64/libcudnn.so{.6.0.21,}
sudo ldconfig
sudo cp include/cudnn.h /usr/local/cuda-8.0/include/
```

4. Install Bazel

```bash
wget https://copr.fedorainfracloud.org/coprs/vbatts/bazel/repo/epel-7/vbatts-bazel-epel-7.repo
sudo cp vbatts-bazel-epel-7.repo /etc/yum.repos.d/
sudo yum install bazel
```

5. Build and install TensorFlow libtensorflow_cc.so

There are many ways to build and install TensorFlow libtensorflow_cc.so, but we have found
the following to currently be the simplest:

```bash
# Help tensorflow_cc find CUDA (only required if using CUDA)
sudo ln -s /usr/local/cuda-8.0 /opt/cuda

# Clone/build tensorflow_cc
git clone https://github.com/FloopCZ/tensorflow_cc.git
cd tensorflow_cc/tensorflow_cc/
mkdir build && cd build
cmake3 -DTENSORFLOW_STATIC=OFF -DTENSORFLOW_SHARED=ON ..
make
sudo make install
```

6. Build MiNiFi - C++ with TensorFlow extension enabled