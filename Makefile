TARGET ?= objtoarr

SRCS := obj-to-js-array.cpp
OBJS := $(addsuffix .o,$(basename $(SRCS)))
DEPS := $(OBJS:.o=.d)

CPPFLAGS ?= -std=c++11 -Wall -O2 -MMD -MP

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@

.PHONY: clean
clean:
	$(RM) $(TARGET) $(OBJS) $(DEPS)

-include $(DEPS)
