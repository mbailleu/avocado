# Avocado

Avocado is secure distributed in-memory key-value store. It leverages SGX to provide trust in an untrusted setting. You can find more details in our USENIX ATC'21 [paper](https://www.usenix.org/conference/atc21/presentation/bailleu).

## Getting started

### Dependencies
* [cmake](https://cmake.org/)
* [fmt](https://fmt.dev/latest/index.html)
* [glog](https://github.com/google/glog)
* [folly](https://github.com/facebook/folly)
* [openssl](https://www.openssl.org/)
* [jemalloc](https://github.com/jemalloc/jemalloc)
* [double-conversion](https://github.com/google/double-conversion)
* [numactl](https://github.com/numactl/numactl)
* [libevent](https://libevent.org/)
* [gflags](https://github.com/gflags/gflags)
* [docopt.cpp](https://github.com/docopt/docopt.cpp)
* [scone](https://sconedocs.github.io/)

### Building
You can build the non-secure version by doing the following
```[bash]
git submodule update --init --recursive
mkdir build
cd build
cmake ..
cmake --build . --parallel
```

You can build the secure version with the scone images. However, you might be required to patch the folly library with the provided [patch](folly-scone.patch). You should also set the SCONE flag in cmake
```[bash]
cmake .. -DSCONE=ON
```

#### Notes
We used a internal development version of scone to run Avocado. We have not tested it with the publicly available version.

### Running
The build process will generate a benchmark application ```build/src/server``` which you can use to run the benchmark, see the ```--help``` flag for information.
