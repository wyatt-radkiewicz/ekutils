#
# Simple makefile by __ e k L 1 p 5 3 d __
#

# Variables
SRCS	:=$(shell find src/ -name "*.c")
BUILD	:=build
OUT	:=$(BUILD)/test
OBJS	:=$(patsubst %.c,$(BUILD)/%.o,$(SRCS))

# Environment variables
CFLAGS	:=$(CFLAGS) -DEK_FEATURE_OFF=1 -O0 -g -std=gnu99
LDFLAGS	:=$(LDFLAGS) -lm

# Build the main executable
$(OUT): $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

# Remember that when this call is evaluated, it is expanded TWICE!
define COMPILE
$$(BUILD)/$(dir $(2))$(1)
	mkdir -p $$(dir $$@)
	$$(CC) $$(CFLAGS) -c $(2) -o $$@
endef

# Go through every source file use gcc to find its pre-reqs and create a rule
$(foreach src,$(SRCS),$(eval $(call COMPILE,$(shell $(CC) $(CFLAGS) -M $(src) | tr -d '\\'),$(src))))

# Clean the project directory
.PHONY: clean
clean:
	rm -rf $(BUILD)

