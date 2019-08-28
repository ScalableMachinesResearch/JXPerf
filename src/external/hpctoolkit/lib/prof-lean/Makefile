include ../../../../../common.mk

ifndef BUILD_DIR
$(error "Must set BUILD_DIR") # this is a Make error
endif

HPC_BUILD_DIR += ${BUILD_DIR}/hpctoolkit

SRCS=$(wildcard *.c)
OBJS=$(addsuffix .o,$(basename $(SRCS)))
OBJS_PATH=$(addprefix $(HPC_BUILD_DIR)/,$(OBJS))

CFLAGS += $(COMMON_FLAGS)
CFLAGS += -std=c99
CFLAGS += -I../..

default: $(BUILD_DIR)/libhpctoolkit.a

$(BUILD_DIR)/libhpctoolkit.a: $(OBJS_PATH)
	ar rcs $(BUILD_DIR)/libhpctoolkit.a $^

$(HPC_BUILD_DIR)/%.o : %.c
	$(MKDIR_P) $(dir $@)
	gcc $(CFLAGS) -c -o $@ $<
