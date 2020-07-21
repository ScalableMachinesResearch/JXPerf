BUILD_DIR=$(shell pwd)/../build
export BUILD_DIR

include ../common.mk

INC_DIRS += $(JAVA_HOME)/include $(JAVA_HOME)/include/linux ./external/hpctoolkit/lib/prof-lean ./external/hpctoolkit ./util ./util/arch/x86-family $(BUILD_DIR)/thirdparty/include

CC := gcc
CXX := g++
LD := g++

export CC CXX LD

SRC_DIRS ?= ./profiler ./perf ./util

SRCS := $(shell find $(SRC_DIRS) -name "*.cpp" -or -name "*.c")
OBJS := $(addsuffix .o,$(basename $(SRCS)))
OBJS_PATH = $(addprefix $(BUILD_DIR)/,$(OBJS))
DEPS := $(OBJS_PATH:.o=.d)

# Variables:

INC_DIRS += $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))


COMMON_FLAGS += -Wall -DUSE_BOOST_UNORDERED_CONTAINER -DENABLE_CALL_PATH_COLLECTION 

CFLAGS += $(COMMON_FLAGS) $(INC_FLAGS) 
CXXFLAGS += $(COMMON_FLAGS) $(INC_FLAGS) -std=c++11

all: $(BUILD_DIR)/libagent.so 

$(BUILD_DIR)/libagent.so: $(OBJS_PATH) $(BUILD_DIR)/libhpctoolkit.a 
	$(LD) -Wl,--whole-archive $(BUILD_DIR)/libhpctoolkit.a -Wl,-no-whole-archive -shared -m64  -Wl,-soname,libagent.so -o $@ $(OBJS_PATH) -Wl,-rpath=$(BUILD_DIR)/thirdparty/lib -L$(BUILD_DIR)/thirdparty/lib -lwatchpoint -lxed -lc -lpfm -ldl -lnuma -L$(JXPerf_HOME)/src/kissmalloc -lkissmalloc -L$(JXPerf_HOME)/preload -lpreload
	cp $(BUILD_DIR)/libagent.so $(BUILD_DIR)/libstop.so

$(BUILD_DIR)/libhpctoolkit.a:
	$(MAKE) -C external

-include $(DEPS)

$(BUILD_DIR)/%.o : %.cpp  
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -c -o $@ $< 
$(BUILD_DIR)/%.o : %.c 
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -MMD -c -o $@ $< 

.PHONY: clean
clean :
	-rm -rf $(BUILD_DIR)/profiler $(BUILD_DIR)/libagent.so

MKDIR_P ?= mkdir -p
export MKDIR_P
