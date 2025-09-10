## About

It is a command line for PIC Programmer K150, written in kanguage C99/C++11 and runnable on Linux platforms.

## Build and execute

Generate the build.
```
cmake -B build . -DCMAKE_BUILD_TYPE=Release
```
Build target.
```
cmake --build build
```
Install PIC database in the build path.
```
cp picpro.dat build/
```
Run the program and show usage.
```
./build/picpro -h
```

