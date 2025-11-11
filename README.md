# UMS - user mode (cooperative) scheduling lib

## Introduction
UMS implements a model of async task execution.

Similar to std::async, it provides a simple APIs for asynchronious execution, but with stable number of threads (workers).  
It tries to minimize interactions with OS and all scheduling is done is user space.

Whole `std::thread` lib is implemented from scratch along with all synchronization primitives: `std::mutex`, `std::condition_variable`, `std::future` etc. 
Therefore, user must use those primitives instead of `std::` ones.

Only important difference between `std::async` and this lib is that user must manually call `yield()` on computation heavy tasks in order to prevent single worker from monopolizing scheduler. This will be explained in details later. 

Supported OS:
* Windows
* Linux

## Prerequisites

* `cmake` - minimum version 3.10
    ```bash
    sudo apt install cmake
    ```
* `clang` and `clang++`
    ```bash
    wget https://apt.llvm.org/llvm.sh
    chmod u+x llvm.sh
    sudo ./llvm.sh 21
    sudo apt install -y libc++-21-dev libc++abi-21-dev
    sudo ln -s /usr/bin/clang-21 /usr/bin/clang
    sudo ln -s /usr/bin/clang++-21 /usr/bin/clang++
    ```

* `clang-tidy` (optional):
    ```bash
    sudo apt install clang-tidy-21
    sudo ln -s /usr/bin/clang-tidy-21 /usr/bin/clang-tidy
    ```
* `Ninja` - for faster build times
    ```bash
    sudo apt install ninja-build
    ```
* For linux `liburing-dev` library is required if you want to use fast I/O:
    ```bash
    sudo apt install liburing-dev
    ```
## Usage

```bash
git clone https://github.com/alcolic-to/ums
cd ums
```

Build:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ 
cmake --build build
```

If you want big speedup in build times use Ninja as build generator.
`cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`

## Testing

First build project (this will also build tests)

There are couple ways to invoke tests

Run `ctest` in the build directory
```bash
cd build
ctest
```

For verbose output use `-V` flag
```bash
cd build
ctest -V
```

To run specific test use `-R` flag
```bash
cd build
ctest -R basic_test
```

## Contributing

Guidelines for contributing to the project.

## License

Information about the project's license.
