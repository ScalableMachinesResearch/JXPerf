BUILD_DIR=$(shell pwd)
export BUILD_DIR

CXX = g++
INC_DIRS = ../src/util

$(BUILD_DIR)/libpreload.so: preload.cpp
	$(CXX) -I$(INC_DIRS) -std=c++11 -fPIC -shared preload.cpp -o $@ 
