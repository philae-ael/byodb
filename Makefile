BUILD_TYPE?=Debug
OUTPUT_DIR?=build
CC:=clang
CXX:=clang++

.PHONY: all run clean iwyu build-all

all: build-all

build-all : $(OUTPUT_DIR)/build.ninja
	ninja -C$(OUTPUT_DIR)

run:  build-all
	$(OUTPUT_DIR)/byodb

clean:
	rm -rf ./$(OUTPUT_DIR)

iwyu: 
	iwyu-tool -p ./compile_commands.json -- -Xiwyu --mapping_file=$(PWD)/iwyu-mapping.yaml

cmake: $(OUTPUT_DIR)/build.ninja

$(OUTPUT_DIR)/build.ninja: CMakeLists.txt
	cmake -DCMAKE_C_COMPILER=$(CC) -DCMAKE_CXX_COMPILER=$(CXX) -DCMAKE_CXX_FLAGS="$(CXXFLAGS)" -DCMAKE_C_FLAGS="$(CFLAGS)" -B$(OUTPUT_DIR) -GNinja
