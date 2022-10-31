# Set the compiler to be used
CC = gcc
# Set the flags to be passed to the compiler regardless of the optimization level
CFLAGS = -march=native -pedantic -Wall -Werror -Wextra -fPIC -Wno-unused-parameter
# Set up the include path
INCPATH = $(ARGOBOTS_INSTALL_DIR)/include ./src/include
# Set up argobots library path
ARGOLIBPATH = $(ARGOBOTS_INSTALL_DIR)/lib
# Set the flags to be passed to the linker
LDFLAGS	= -labt -shared
# Set the optimization level for the release build
OPTFLAGS = -Ofast
# Set the flags for the debug build
DBGFLAGS = -O0 -Og -g3 -ggdb -fsanitize=thread 

# Gather all the source files
SRC = $(wildcard src/*.c)
# Set the name of the target library
LIB = libargolib.so

# List all the objects to be compiled for the release build
OBJECTS = $(patsubst src/%.c,release/build/%.o,$(SRC))
# List the target for the release build
TARGET = release/lib/$(LIB)

# List all the objects for the debug build
DEBUG_OBJECTS = $(patsubst src/%.c,debug/build/%.o,$(SRC))
# List the target for the debug build
DEBUG_TARGET = debug/lib/$(LIB)

.PHONY: release
release: $(TARGET)								# Set the default target as release for make
.PHONY: debug
debug: $(DEBUG_TARGET)								# Set the debug target for make

$(TARGET): $(OBJECTS)								# Specify how to compile TARGET
	$(CC) -L$(ARGOLIBPATH) $(CFLAGS) $(OPTFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)
$(OBJECTS): $(SRC)								# Specify how to compile OBJECTS
	$(CC) $(foreach inc_path,$(INCPATH),-I$(inc_path)) $(CFLAGS) $(OPTFLAGS) -c $^ -o $@

$(DEBUG_TARGET) : $(DEBUG_OBJECTS)						# Specify how to compile DEBUG_TARGET
	$(CC) -L$(ARGOLIBPATH) $(CFLAGS) $(DBGFLAGS) -o $(DEBUG_TARGET) $(DEBUG_OBJECTS) $(LDFLAGS)
$(DEBUG_OBJECTS): $(SRC)							# Specify how to compile DEBUG_OBJECTS
	$(CC) $(foreach inc_path,$(INCPATH),-I$(inc_path)) $(CFLAGS) $(DBGFLAGS) -c $^ -o $@

.PHONY: help									# Specify the help target which prints the usage
help:
	@echo "Usage: make [TARGET] [-j[num_threads]]"
	@echo "TARGET		: Specifies what to do. Default value is release"
	@echo "j		: Number of compilation jobs to run. Default value is 1. If num_threads is not specified, run number_of_cores+1 jobs"
	@echo "TARGET:"
	@echo "	release		: Builds the library with all the optimizations enabled"
	@echo "	debug		: Builds the library with debug information"
	@echo "	clean		: Removes all the files that were built"
	@echo "	help		: Displays this message"

.PHONY: clean
clean:										# Specify the clean target
	rm -rf $(TARGET)
	rm -rf $(OBJECTS)
	rm -rf $(DEBUG_TARGET)
	rm -rf $(DEBUG_OBJECTS)
