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
* `clang` and `clang++`
* `Ninja` - for faster build times
* For linux 'liburing-dev' library is required:
    - ubuntu: `sudo apt install liburing-dev`

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
