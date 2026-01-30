cmake_minimum_required(VERSION 2.8)

project(lattice)
enable_testing()

# set(CMAKE_C_FLAGS "-std=gnu99 -fPIC -lpmem -O3 -lpthread")
#set(CMAKE_C_FLAGS "-std=gnu99 -fPIC   -fopenmp -lpmem -O3 -lpthread")
# set(CMAKE_C_FLAGS "-std=gnu99 -fPIC -fopenmp -lpmem  -lpthread")
# set(CMAKE_C_FLAGS "-std=gnu99 -fPIC -lpmem  -lpthread")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -fPIC -fopenmp -O3")

if(CMAKE_COMPILER_IS_GNUCXX)
  add_definitions(${CMAKE_CXX_FLAGS} "-fno-reorder-functions")
  add_definitions(${CMAKE_CXX_FLAGS} "-fno-inline")
endif()
include_directories(. utils tests)

add_subdirectory(tests)
add_subdirectory(benchmarks)
add_subdirectory(sqlite)

set(SOURCE_FILES
	cont.c
    nlmapper.c
	slab.c
    container_version.c
    checkpoint.c
    snapshot.c
    restore.c
    alloc.c
	page_alloc.c
	sfhandler.c
	fixptr.c
    fixmapper.c
    atomics.c
    closure.c
    pointer_locations.c
    hash.c
    htable.c
    debug.c
    out.c
    stats.c
)

add_library(lattice STATIC ${SOURCE_FILES})

target_link_libraries(lattice 
    pmem 
    pthread
)
#target_link_libraries(ss)



