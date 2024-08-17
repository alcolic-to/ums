# COS - cooperative scheduling lib

## Introduction

Supported OS:
* Windows
* Linux

## Prerequisites

* `cmake` - minimum version 3.10
* `clang` and `clang++`
* `Ninja` - for faster build times
* For linux `liburing-dev` package is required

## Usage

```bash
git clone https://github.com/aleksandarcolic22414/cos-cooperative-scheduling
cd cos-cooperative-scheduling
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
