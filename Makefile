CFLAGS := \
	-Wall -std=gnu11 \
	-I. -MMD -pthread

OBJS := \
	src/alloc.o \
	src/core.o \
	src/list.o \
	src/utils.o \
	src/gc_thread_funcs.o

deps := $(OBJS:%.o=%.d)

TESTS := \
	tests/dummy \
	tests/string-separate

tests/%: tests/%.o
	$(CC) $(CFLAGS) -o $@ $^ $(OBJS)
deps += $(TESTS:%=%.d)

all: $(OBJS)

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    PRINTF = printf
else
    PRINTF = env printf
endif

PASS_COLOR = \e[32;01m
NO_COLOR = \e[0m
tests/%.done: tests/%
	@$(PRINTF) "*** Validating $< ***\n"
	@valgrind $< && $(PRINTF) "\t$(PASS_COLOR)[ Verified ]$(NO_COLOR)\n"
check: $(OBJS) $(addsuffix .done, $(TESTS))

clean:
	$(RM) $(deps) $(OBJS) $(TESTS)

-include $(deps)
