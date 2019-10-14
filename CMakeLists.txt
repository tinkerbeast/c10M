cmake_minimum_required(VERSION 3.5)
project(myjunk VERSION 0.0.1)


# TODO: cross compile includes

# === Define defaults ========================================================

# default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# c and cpp version
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_EXTENSIONS OFF)

# compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_compile_options(-pedantic -Wall -Wextra -Wconversion -Wno-strict-aliasing)
    # TODO -Wsign-conversion fails because of gtest
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wnon-virtual-dtor")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-pedantic -Wall -Wextra -Wdocumentation -Weffc++ -Wshorten-64-to-32 -Wconversion)
endif()

# miscellaneous

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# === Print build platform informations ======================================

message(STATUS "CMAKE_VERSION = ${CMAKE_VERSION}")
message(STATUS "CMAKE_SYSTEM_NAME = ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMAKE_SYSTEM_VERSION = ${CMAKE_SYSTEM_VERSION}")
message(STATUS "CMAKE_SYSTEM_PROCESSOR = ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_CXX_FLAGS = ${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_CXX_COMPILER_ID = ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "CMAKE_CXX_COMPILER_VERSION = ${CMAKE_CXX_COMPILER_VERSION}")


# === Source dependencies ====================================================

set(DEV_DEPENDENCIES "")

# library pthreads
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)
list(APPEND DEV_DEPENDENCIES pthread)


# === Test dependencies ======================================================

set(TEST_DEPENDENCIES "")

#  library google/googletest
#add_subdirectory("${PROJECT_SOURCE_DIR}/thirdparty/googletest/googletest" EXCLUDE_FROM_ALL)
#include_directories ("${PROJECT_SOURCE_DIR}/thirdparty/googletest/googletest/include")
#list(APPEND TEST_DEPENDENCIES gtest gtest_main)


# === Source module helper functions =========================================

function(my_add_cxx_lib lib_name lib_subdir)
    # add all files in the subdirectory
    file(GLOB_RECURSE SOURCES ${lib_subdir}/*.cpp)
    foreach(filename ${SOURCES})
        list(APPEND OBJECTS ${filename})
    endforeach(filename ${SOURCES})
    # make a static and dynamic library
    add_library(${lib_name} SHARED ${OBJECTS})
    target_link_libraries(${lib_name} PRIVATE ${DEV_DEPENDENCIES})
    add_library(${lib_name}-static STATIC ${OBJECTS})
endfunction()

function(my_add_c_lib lib_name lib_subdir)
    # add all files in the subdirectory
    file(GLOB_RECURSE SOURCES ${lib_subdir}/*.c)
    foreach(filename ${SOURCES})
        list(APPEND OBJECTS ${filename})
    endforeach(filename ${SOURCES})
    # make a static and dynamic library
    add_library(${lib_name} SHARED ${OBJECTS})
    target_link_libraries(${lib_name} PRIVATE ${DEV_DEPENDENCIES})
    add_library(${lib_name}-static STATIC ${OBJECTS})
endfunction()

function(my_add_dev_exec exec_name exec_file lib_list)
    # the dynamic executable
    add_executable(${exec_name} ${exec_file})
    target_link_libraries(${exec_name} ${DEV_DEPENDENCIES} ${lib_list})
    # the static executable # TODO: the static build is broken because of adding dev-dependencies statically
    foreach(lib_name ${lib_list})
        list(APPEND static_lib_list ${lib_name}-static)
    endforeach(lib_name)
    add_executable(${exec_name}-static EXCLUDE_FROM_ALL ${exec_file}) # TODO EXCLUDE_FROM_ALL is workaround to avoid static building for now
    target_link_libraries(${exec_name}-static ${DEV_DEPENDENCIES} ${static_lib_list}) 
endfunction()


# === Test module helper functions ===========================================

add_custom_target(test-run)

function(my_add_test_exec exec_name test_module src_targets)
    # compile the test lib
    my_add_cxx_lib(${exec_name}-testlib "${PROJECT_SOURCE_DIR}/test/${test_module}")
    # build the test runn
    add_executable(${exec_name} EXCLUDE_FROM_ALL "${PROJECT_SOURCE_DIR}/test/${test_module}/main.cpp")
    target_link_libraries(${exec_name} ${DEV_DEPENDENCIES} ${TEST_DEPENDENCIES} ${exec_name}-testlib ${src_targets})
    #
    add_custom_target(${exec_name}-run
        COMMAND ${exec_name} #cmake 2.6 required
        DEPENDS ${exec_name}-testlib ${src_targets} "${PROJECT_SOURCE_DIR}/test/${test_module}/main.cpp"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
    # run it as part of make test
    add_dependencies(test-run ${exec_name}-run)
endfunction()


# === Add source modules =====================================================

include_directories ("${PROJECT_SOURCE_DIR}/src/httpio")
    
set(DEFAULT_BUILD_LIBS "")

my_add_c_lib(httpiolib "${PROJECT_SOURCE_DIR}/src/httpio")
list(APPEND DEFAULT_BUILD_LIBS httpiolib)

my_add_dev_exec(httpio "${PROJECT_SOURCE_DIR}/src/main.c" "${DEFAULT_BUILD_LIBS}")


# === Add test modules =======================================================

#my_add_test_exec(misc-test misc misc)


