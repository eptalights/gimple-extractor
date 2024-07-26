CXX = g++
CXXFLAGS = -fPIC
LDFLAGS = -shared
SRC_DIR = src
EXT_DIR = $(SRC_DIR)/ext
BIN_DIR = bin

# Flags for the C++ compiler: enable C++11 and all the warnings, -fno-rtti is required for GCC plugins
CXXFLAGS += -std=c++11 -Wall -fno-rtti 
# Workaround for an issue of -std=c++11 and the current GCC headers
CXXFLAGS += -Wno-literal-suffix

# Determine the plugin-dir and add it to the flags
PLUGINDIR=$(shell $(CXX) -print-file-name=plugin)
CXXFLAGS += -I$(PLUGINDIR)/include

# Source files
SRCS = $(SRC_DIR)/gimple_extractor.cc $(EXT_DIR)/json11.cpp $(EXT_DIR)/msgpack11.cpp \
       $(SRC_DIR)/data_formatter.cc $(SRC_DIR)/data_formatter_json.cc $(SRC_DIR)/data_formatter_msgpack.cc

# Object files
OBJS = $(SRCS:%.cc=$(BIN_DIR)/%.o)

# Target
TARGET = $(BIN_DIR)/gimple_extractor.so

# Rules
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

$(BIN_DIR)/%.o: %.cc
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf $(BIN_DIR)

check: $(TARGET)
	$(CXX) -fplugin=$(TARGET) -c -x c++ /dev/null -o /dev/null

docker-shell-14.1.0:
	docker run --rm -it --entrypoint /bin/bash -v ${PWD}/:/gimple_extractor gcc:14.1.0

docker-build-14.1.0:
	docker run --rm -it --entrypoint /gimple_extractor/build_plugin.sh -v ${PWD}/:/gimple_extractor gcc:14.1.0

docker-shell-13.3.0:
	docker run --rm -it --entrypoint /bin/bash -v ${PWD}/:/gimple_extractor gcc:13.3.0

docker-build-13.3.0:
	docker run --rm -it --entrypoint /gimple_extractor/build_plugin.sh -v ${PWD}/:/gimple_extractor gcc:13.3.0

docker-shell-13.2.0:
	docker run --rm -it --entrypoint /bin/bash -v ${PWD}/:/gimple_extractor gcc:13.2.0

docker-build-13.2.0:
	docker run --rm -it --entrypoint /gimple_extractor/build_plugin.sh -v ${PWD}/:/gimple_extractor gcc:13.2.0

docker-shell-12.4.0:
	docker run --rm -it --entrypoint /bin/bash -v ${PWD}/:/gimple_extractor gcc:12.4.0

docker-build-12.4.0:
	docker run --rm -it --entrypoint /gimple_extractor/build_plugin.sh -v ${PWD}/:/gimple_extractor gcc:12.4.0

docker-shell-11.4.0:
	docker run --rm -it --entrypoint /bin/bash -v ${PWD}/:/gimple_extractor gcc:11.4.0

docker-build-11.4.0:
	docker run --rm -it --entrypoint /gimple_extractor/build_plugin.sh -v ${PWD}/:/gimple_extractor gcc:11.4.0

docker-shell-10.5.0:
	docker run --rm -it --entrypoint /bin/bash -v ${PWD}/:/gimple_extractor gcc:10.5.0

docker-build-10.5.0:
	docker run --rm -it --entrypoint /gimple_extractor/build_plugin.sh -v ${PWD}/:/gimple_extractor gcc:10.5.0

docker-shell-10.4.0:
	docker run --rm -it --entrypoint /bin/bash -v ${PWD}/:/gimple_extractor gcc:10.4.0

docker-build-10.4.0:
	docker run --rm -it --entrypoint /gimple_extractor/build_plugin.sh -v ${PWD}/:/gimple_extractor gcc:10.4.0

.PHONY: all clean
