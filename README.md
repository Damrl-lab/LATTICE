# LATTICE: Efficient In-Memory DNN Model Versioning

This repo contains source code associated with the paper "LATTICE: Efficient In-Memory DNN Model Versioning", published in SYSTOR'25: Proceedings of the 18th ACM International Systems and Storage Conference. 
[ACM Digital Library](https://dl.acm.org/doi/10.1145/3757347.3759139)

LATTICE is a persistent memory library based on the library LibPM. LATTICE has the feature of checkpointing data in persistent memory, maintaining multiple versions of persistent data, accessing any version of the previously checkpointed data. All the previously checkpointed versions can be accessed for reading. Only the last checkpointed version is currently stable for writing and further version creations. 

LATTICE versioning works on the methodology "Copy on Checkpoint". LATTICE maintains a version where all the write happens in place. We call it the working version. Whenever a checkpoint is called, LATTICE performs the flush+drain operation on the modified pages. When the flush+drain operation is done, it is the persistent point: the modified data has been persisted. At this point LATTICE returns the persistence update to the application and starts creating a shadow copy of the working version in the background. Until the shadow copying is not finished and persisted, further modification on the working version and checkpoint cannot be initiated by the application. The shadhow copied version is denoted as the last checkpointed version. 

All the currently available LATTICE version apis related examples can be found in tests/test_version.c 

To include the LATTICE library in a project:
* Install the libpem library in the system.
* Complile LATTICE.
* Add the LATTICE.a file to the project.
* Add appropiate header files before accessing the codes.
   (see /tests/test_version.c example file)

Required Software
=================

    * libpmem: Libpem is a persistent memory library by intel. In this project
    libpem.h is used for flushing and fencing data in persistent memory.
    https://docs.pmem.io/persistent-memory/getting-started-guide/installing-pmdk/installing-pmdk-using-linux-packages

    https://pmem.io/pmdk/libpmem/ 

    * CMake: CMake is cross-platform free and open-source software for managing
        the build process of software using a compiler-independent method.


How to compile the library
==========================

First, get a copy of the source. Assuming that we store the path to the code
folder in a variable called PMLIB_TOP, we compile the code by issuing the
following commands:

    $ cd $PMLIB_TOP

Now we create a folder in which we will compile the code. This is convenient

because it separates the binaries from the source.

    $ mkdir build
    $ cd build
    $ cmake ../src
    $ make

If everything went well, we should have a LATTICE.a file in the build folder
along with other test files in $PMLIB_TOP/build/tests

NOTE: If you want to debug the code, you should consider compiling it with
debugging symbols. This can be done by telling cmake that we want a Debug build
type like this.

    cmake -DCMAKE_BUILD_TYPE=Debug ../src
    to build with gprog 
    cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-pg -DCMAKE_EXE_LINKER_FLAGS=-pg -DCMAKE_SHARED_LINKER_FLAGS=-pg ../src


How to test the versioning features
=======

Test code related to version features can be found in tests/test_version. All the currently available
version apis can be found implemented there. 

Benchmark
=========

The benchmark folder contains benchmarks use to tests the performance of the
library. As of now, the only comparison point we use is TPL.

Settings
========

The library provides both logging and stats. These could be very helpful when
debugging the code but it can also add a significant overhead. Both logging and
stats can be disabled by updating the settings.h file.

Environment Variables
=====================

The following environment variables could be set to change the default behavior
of the library.

    PMLIB_CLOSURE=0     Disables the closure computation. (Defaults 1)

    PMLIB_FIX_PTRS=0    No metadata about persistent pointers is store and the
                        pointers are not fixed after a container restore. This
                        only applies when we can guarantee that the container
                        was mmap at the same address on which is was created.
                        (Defaults 1)

    PMLIB_LOG_LEVEL=n   Sets the log level to n. Msg(s) with log-level bigger
                        than n will not be logged.

    PMLIB_LOG_FILE=/path/to/file
                        Define the log file

    PMLIB_INIT_SIZE=n   Initializes the container file to n bytes.

    PMLIB_CONT_FILE=/path/to/file
                        Create container at the given path

    PMLIB_USE_FMAPPER=1 Use Fixed Mapper page allocator (default)

    PMLIB_USE_NLMAPPER=1
                        Use Non-Linear Mapper page allocator


Running tests with large containers
===================================

The current design maps portions of the backend-file into the process address
space. When large containers are used, it's possible to exceed the default
maximum number of mmap regions per process. To solve this problem we change
this limit following these instructions.

    To check the current value

        $ sysctl  vm.max_map_count

    To change the value to n

        # sysctl -w vm.max_map_count=n

Citation
=========
```
@inproceedings{10.1145/3757347.3759139,
author = {Saha, Manoj P. and Ghosh, Ashikee and Rangaswami, Raju and Wu, Yanzhao and Bhimani, Janki},
title = {LATTICE: Efficient In-Memory DNN Model Versioning},
year = {2025},
isbn = {9798400721199},
publisher = {Association for Computing Machinery},
address = {New York, NY, USA},
url = {https://doi.org/10.1145/3757347.3759139},
doi = {10.1145/3757347.3759139},
booktitle = {Proceedings of the 18th ACM International Systems and Storage Conference},
pages = {16–29},
numpages = {14},
keywords = {checkpointing, deep neural network, non-volatile memory, persistent memory},
location = {Virtual, Israel},
series = {SYSTOR '25}
}
```