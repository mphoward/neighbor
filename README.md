# neighbor

A library of CUDA implementations of neighbor search algorithms.
Currently, the compressed LBVH and uniform grid are implemented.
For additional details, please refer to the following publications:

## Dependencies

* [HOOMD-blue](http://bitbucket.org/glotzer/hoomd-blue) >= 2.4.1
* CMake >= 2.8.0
* A CUDA toolkit compatible with HOOMD-blue and an NVIDIA GPU with
  compute capability >= 3.5.

## Compiling

The neighbor library is intended to be compiled similarly to a HOOMD-blue
plugin. First, you must install HOOMD-blue and its dependencies with CUDA
enabled. Then, add your HOOMD-blue installation to your `PYTHONPATH`.
The neighbor library should automatically find HOOMD-blue. From the current
directory,
```
export PYTHONPATH=/path/to/hoomd/2.4.1
mkdir build && cd build
cmake ..
```
Set any CMake flags that are appropriate for your target architecture
(e.g., `CUDA_ARCH_LIST`). Most options will have already been fixed by
your HOOMD-blue installation. Then, simply compile and install,
```
make install
```
If you have difficulties with the CMake configuration, refer to the HOOMD-blue
documentation for hints regarding compilation.

The following will be found in your installation:

* `bin`: benchmark executables
* `include`: header files for the neighbor library
* `lib`: the neighbor library

Note that the neighbor library uses a fixed `RPATH` at installation.
If you move your HOOMD-blue installation, you will need to recompile
the library.

## Testing

You can validate your installation using the included test suite.
Make sure that you are on a machine that has at least one available GPU,
and then run
```
make test
```
or a specific test using `ctest`.
