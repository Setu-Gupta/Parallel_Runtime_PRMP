# Set the compiler to be used
CXX = g++
# Set the flags to be passed to the compiler regardless of the optimization level
CFLAGS = -march=native -pedantic -Wall -Werror -Wextra
# Set the optimization level for the release build
OPTFLAGS = -Ofast
# Set the flags for the debug build
DBGFLAGS = -Og -g3 -ggdb -fsanitize=address

# Gather all the source files
SRC = $(wildcard *.cpp)

# List all the objects to be compiled
OBJECTS = $(patsubst %.cpp,build/release/%.o,$(SRC))
# List the target binaries to be generated
TARGETS = $(patsubst %.cpp,bin/release/%,$(SRC))
# List all the debug objects to be compiled
DEBUG_OBJECTS = $(patsubst %.cpp,build/debug/%.o,$(SRC))
# List the debug target binaries to be generated
DEBUG_TARGETS = $(patsubst %.cpp,bin/debug/%,$(SRC))

.PHONY: release
release: $(TARGETS)								# Set the default target as release for make
.PHONY: debug
debug: $(DEBUG_TARGETS)								# Set the debug target for make

$(TARGETS): $(OBJECTS)								# Specify how to compile TARGETS
	$(CXX) $(OPTFLAGS) $^ -o $@
$(OBJECTS): $(SRC)								# Specify how to compile OBJECTS
	$(CXX) $(CFLAGS) $(OPTFLAGS) -c $^ -o $@

$(DEBUG_TARGETS) : $(DEBUG_OBJECTS)						# Specify how to compile DEBUG_TARGETS
	$(CXX)$(DBGFLAGS) $^ -o $@
$(DEBUG_OBJECTS): $(SRC)							# Specify how to compile DEBUG_OBJECTS
	$(CXX) $(CFLAGS) $(DBGFLAGS) -c $^ -o $@

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
	rm -rf build/release/*
	rm -rf build/debug/*
	rm -rf bin/release/*
	rm -rf bin/debug/*
