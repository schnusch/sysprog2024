EXAMPLE_SRC := leak.c
EXAMPLE_OBJ := $(EXAMPLE_SRC:.c=.o)
EXAMPLE := $(EXAMPLE_SRC:.c=)

WRAPPER_SRC := mallocWrap.c
WRAPPER_OBJ := $(WRAPPER_SRC:.c=.o)
WRAPPER := $(WRAPPER_SRC:.c=.so)

CFLAGS = -Wall -Wextra -g3 -std=c11


# make lacks an implicit rule for shared objects :-(
%.so: %.o
	$(LINK.o) -fPIC -shared -o $@ $^ $(LOADLIBES) $(LDLIBS)

test : all
	LD_PRELOAD=$(PWD)/mallocWrap.so $(PWD)/$(EXAMPLE)

all : $(EXAMPLE) $(WRAPPER)

$(EXAMPLE) : $(EXAMPLE_OBJ)

$(WRAPPER) : $(WRAPPER_OBJ)
$(WRAPPER) : LDLIBS += -ldl
$(WRAPPER) : LDFLAGS += -Wl,-no-undefined

$(WRAPPER_OBJ) : CFLAGS += -fPIC

clean cleanall :
	$(RM) $(EXAMPLE) $(EXAMPLE_OBJ) $(WRAPPER) $(WRAPPER_OBJ)

.PHONY : test all clean cleanall
