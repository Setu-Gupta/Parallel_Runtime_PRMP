# Set the compiler to be used
CC = gcc
# Set the flags to be passed to the compiler regardless of the optimization level
CFLAGS = -march=native -pedantic -Wall -Werror -Wextra
# Set the flags to be passed to the linker
LDFLAGS	= -largolib -labt
# Set the optimization level for the release build
OPTFLAGS = -Ofast
# Set the flags for the debug build
DBGFLAGS = -Og -g3 -ggdb -fsanitize=address
# Set up the include path
INCPATH = $(ARGOLIB_INSTALL_DIR)/include $(ARGOBOTS_INSTALL_DIR)/include
# Set up argobots library path
LIBPATH = $(ARGOBOTS_INSTALL_DIR)/lib
# Set up argolib library path for debug build
DBGARGOLIB = $(ARGOLIB_INSTALL_DIR)/debug/lib
# Set up argolib library path for release build
RELARGOLIB = $(ARGOLIB_INSTALL_DIR)/release/lib

# Gather all the source files
SRC = $(wildcard *.c)

# List all the objects to be compiled
OBJECTS = $(patsubst %.c,build/release/%.o,$(SRC))
# List the target binaries to be generated
TARGETS = $(patsubst %.c,bin/release/%,$(SRC))
# List all the debug objects to be compiled
DEBUG_OBJECTS = $(patsubst %.c,build/debug/%.o,$(SRC))
# List the debug target binaries to be generated
DEBUG_TARGETS = $(patsubst %.c,bin/debug/%,$(SRC))

release: $(TARGETS)								# Set the default target as release for make
debug: $(DEBUG_TARGETS)								# Set the debug target for make

$(TARGETS): $(OBJECTS)								# Specify how to compile TARGETS
	$(CC) -L$(RELARGOLIB) -L$(LIBPATH) $(LDFLAGS) $^ -o $@
$(OBJECTS): $(SRC)								# Specify how to compile OBJECTS
	$(CC) -L$(RELARGOLIB) -L$(LIBPATH) $(foreach inc_path,$(INCPATH),-I$(inc_path)) $(CFLAGS) $(LDFLAGS) $(OPTFLAGS) -c $^ -o $@

$(DEBUG_TARGETS) : $(DEBUG_OBJECTS)						# Specify how to compile DEBUG_TARGET
	$(CC) -L$(DBGARGOLIB) -L$(LIBPATH) $(LDFLAGS) $(DBGFLAGS) $^ -o $@
$(DEBUG_OBJECTS): $(SRC)							# Specify how to compile DEBUG_OBJECTS
	$(CC) -L$(DBGARGOLIB) -L$(LIBPATH) $(foreach inc_path,$(INCPATH),-I$(inc_path)) $(DBGFLAGS) $(LDFLAGS) $(DBGFLAGS) -c $^ -o $@

.PHONY: help									# Specify the help target which prints the usage
help:
	@echo "Usage: make [TARGET] [-j[num_threads]]"
	@echo "TARGET		: Specifies what to do. Default value is release"
	@echo "j		: Number of compilation jobs to run. Default value is 1. If num_threads is not specified, run number_of_cores+1 jobs"
	@echo "TARGET:"
	@echo "	release		: Builds the binaries with all the optimizations enabled"
	@echo "	debug		: Builds the binaries with debug information"
	@echo "	clean		: Removes all the files that were built"
	@echo "	help		: Displays this message"

.PHONY: clean
clean:										# Specify the clean target
	rm -rf $(TARGETS)
	rm -rf $(OBJECTS)
	rm -rf $(DEBUG_TARGETS)
	rm -rf $(DEBUG_OBJECTS)
