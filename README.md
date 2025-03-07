# Native RPC Communication Library

Cross-platform and cross-language communication library in C++. Part of larger [Native RPC](https://github.com/nativerpc) framework. See [nrpc-examples/README.md](https://github.com/nativerpc/nrpc-examples) for more information.

# Prerequisites

Ensuring up-to-date C++ tooling in Linux.

```
sudo apt install cmake
sudo apt install build-essential
```

Ensuring up-to-date C++ tooling in Windows:

- Install Visual Studio Code Community 2022
- Install CMake

# Configuration and dependency build

Project configuration and dependency build.

```
cmake -B build
```

# Normal build

Normal Build.

```
cmake --build build -j20
```

# Manual tests

Manual and automated tests.

```
build\bin\test_json.exe
build\bin\test_zmq.exe
build\bin\test_show.exe port=9001
```

