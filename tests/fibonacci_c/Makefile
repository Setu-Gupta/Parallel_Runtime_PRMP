# Set the compiler to be used
CC = gcc
# Set the flags to be passed to the compiler regardless of the optimization level
CFLAGS = -march=native -pedantic -Wall -Werror -Wextra
# Set the flags to be passed to the linker
LDFLAGS	= -largolib -labt
# Set the optimization level for the release build
OPTFLAGS = -Ofast
# Set the flags for the debug build
DBGFLAGS = -O0 -Og -g3 -ggdb
# Set up the include path
INCPATH = $(ARGOLIB_INSTALL_DIR)/include $(ARGOBOTS_INSTALL_DIR)/include
# Set up argolib library path for debug build
DBGARGOLIB = $(ARGOLIB_INSTALL_DIR)/debug/lib
# Set up argolib library path for release build
RELARGOLIB = $(ARGOLIB_INSTALL_DIR)/release/lib
# Set up argobots library path
ARGOLIB = $(ARGOBOTS_INSTALL_DIR)/lib

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

.PHONY: release
release: $(TARGETS)								# Set the default target as release for make
.PHONY: debug
debug: $(DEBUG_TARGETS)								# Set the debug target for make
.PHONY: sanitize								# Create a new target which uses address sanitizer along with debug flags
sanitize: DBGFLAGS += -fsanitize=address
sanitize: debug_clean debug
.PHONY: debug_clean
debug_clean:
	rm -rf $(DEBUG_TARGET)
	rm -rf $(DEBUG_OBJECTS)

$(TARGETS): $(OBJECTS)								# Specify how to compile TARGETS
	$(CC) -L$(ARGOLIB) -L$(RELARGOLIB) $(OPTFLAGS) $^ -o $@ $(LDFLAGS)
$(OBJECTS): $(SRC)								# Specify how to compile OBJECTS
	$(CC) -L$(ARGOLIB) -L$(RELARGOLIB) $(foreach inc_path,$(INCPATH),-I$(inc_path)) $(CFLAGS) $(OPTFLAGS) -c $^ -o $@ $(LDFLAGS)

$(DEBUG_TARGETS) : $(DEBUG_OBJECTS)						# Specify how to compile DEBUG_TARGETS
	$(CC) -L$(ARGOLIB) -L$(DBGARGOLIB) $(DBGFLAGS) $^ -o $@ $(LDFLAGS)
$(DEBUG_OBJECTS): $(SRC)							# Specify how to compile DEBUG_OBJECTS
	$(CC) -L$(ARGOLIB) -L$(DBGARGOLIB) $(foreach inc_path,$(INCPATH),-I$(inc_path)) $(DBGFLAGS) $(DBGFLAGS) -c $^ -o $@ $(LDFLAGS)

.PHONY: help									# Specify the help target which prints the usage
help:
	@echo "Usage: make [TARGET] [-j[num_threads]]"
	@echo "TARGET		: Specifies what to do. Default value is release"
	@echo "j		: Number of compilation jobs to run. Default value is 1. If num_threads is not specified, run number_of_cores+1 jobs"
	@echo "TARGET:"
	@echo "	release		: Builds the binaries with all the optimizations enabled"
	@echo "	debug		: Builds the binaries with debug information"
	@echo "	sanitize	: Builds the library with debug information and address sanitizer"
	@echo "	clean		: Removes all the files that were built"
	@echo "	help		: Displays this message"

.PHONY: clean
clean:										# Specify the clean target
	rm -rf $(TARGETS)
	rm -rf $(OBJECTS)
	rm -rf $(DEBUG_TARGETS)
	rm -rf $(DEBUG_OBJECTS)
