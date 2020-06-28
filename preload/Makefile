BUILD_DIR=$(shell pwd)/../build
export BUILD_DIR

$(BUILD_DIR)/libpreload.so: preload.cpp
	g++ -std=c++11 -fPIC -shared preload.cpp -o $@ 
