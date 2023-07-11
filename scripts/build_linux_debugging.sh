#!/usr/bin/env bash

# build OSC with all the necessary debugging bells and whistles
# that are used during a release

CC=clang CXX=clang++ CCFLAGS=-fsanitize=address CXXFLAGS=-fsanitize=address cmake -S third_party/ -B osc-deps-build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=${PWD}/osc-deps-install
cmake --build osc-deps-build/ -v -j$(nproc)

CC=clang CXX=clang++ CCFLAGS=-fsanitize=address CXXFLAGS=-fsanitize=address cmake -S . -B osc-build -DCMAKE_BUILD_TYPE=Debug -DOSC_FORCE_ASSERTS_ENABLED=ON -DOSC_FORCE_UNDEFINE_NDEBUG=ON -DCMAKE_PREFIX_PATH=${PWD}/osc-deps-install -DCMAKE_INSTALL_PREFIX=${PWD}/osc-install
cmake --build osc-build -j$(nproc)

# run tests
./osc-build/tests/oscar/testoscar
ASAN_OPTIONS=detect_leaks=0 ./osc-build/tests/OpenSimCreator/testopensimcreator  --gtest_filter=*CanAddAnyForceWithoutASegfault*