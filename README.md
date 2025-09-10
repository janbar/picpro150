## About

It is a command line for PIC Programmer K150, written in kanguage C99/C++11 and runnable on Linux platforms.

For now, other tools allow you to use the K150 programmer under Linux. But their programming language is a bit shaky for me.
So I recreated the tool in C/C++, adding the features I need.
The source code is maintainable by myself or by anyone with basic C/C++ knowledge.
The source code is intentionally unoptimized to be readable by anyone.

It is designed for Linux, but can easily be adapted to other platforms supporting a serial port.

## Build and execute

First install tools to build it. The requirements are cmake (>= 3.8.2) and gcc-c++ (>= 8.5.0) or clang.
As example, install the requirements by typing the following.

On Ubuntu:
`apt install cmake gcc g++ cpp`

On Fedora:
`yum install cmake gcc-c++ gcc cpp`

Then generate the CMake project from the source path.
```
cmake -B build . -DCMAKE_BUILD_TYPE=Release
```
Build target.
```
cmake --build build
```
By default the program loads the PIC database (picpro.dat) in the same path of the launched binary.
Also you can specify the file path as needed with the option -d (see usage page). In the example below
I copy the database file in the build folder, and then run the program from it.
```
cp picpro.dat build/
```
Run the program and show usage.
```
cd build
./picpro -h
```

