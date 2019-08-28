include ../common.mk

########################################
# Don't touch anything after this line #
########################################

BUILD_DIR=$(shell pwd)/../build
export BUILD_DIR

CC := gcc
CXX := g++
LD := g++

CXXFLAGS = -std=c++11
CFLAGS = -std=c99

export CC CXX LD

SRC_DIRS ?= ./

SRCS := $(shell find $(SRC_DIRS) -name "*.cpp" -or -name "*.c")
OBJS := $(addsuffix .o,$(basename $(SRCS)))
OBJS_PATH = $(addprefix $(BUILD_DIR)/,$(OBJS))
DEPS := $(OBJS_PATH:.o=.d)

# Variables:

INC_DIRS += $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

COMMON_FLAGS += -Wall

CFLAGS += $(COMMON_FLAGS) $(INC_FLAGS)
CXXFLAGS += $(COMMON_FLAGS) $(INC_FLAGS) -std=c++11

all: $(BUILD_DIR)/libpreload.so 

$(BUILD_DIR)/libpreload.so: $(OBJS_PATH)
	$(LD) -shared -m64 -fPIC -Wl,-soname,libpreload.so -o $@ $(OBJS_PATH) $(LDFLAGS)  -ldl -lm


-include $(DEPS)

#$(BUILD_DIR)/%.o : %.cpp  
#	$(MKDIR_P) $(dir $@)
#	$(CXX) $(CXXFLAGS) -MMD -c -o $@ $<
$(BUILD_DIR)/%.o : %.c 
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -c -o $@ $<

.PHONY: clean
clean :
	-rm -rf $(BUILD_DIR)

MKDIR_P ?= mkdir -p
export MKDIR_P
