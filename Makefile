CXX      := g++
CXXFLAGS := -std=c++17 -O3 -Wall -Wextra -pthread
INCLUDE  := -Iinclude
TARGET   := main
BUILD_DIR := build
SRC_DIR  := src
SOURCES  := $(SRC_DIR)/main.cpp
OBJECTS  := $(SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# 默认目标
all: $(TARGET)

# 创建输出目录并编译
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

# 链接生成可执行文件
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@

# 快速编译（不走 build 目录）
simple:
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(SRC_DIR)/main.cpp -o $(TARGET)

# 清理构建产物
clean:
	rm -rf $(BUILD_DIR) $(TARGET) *.dat *.log

.PHONY: all clean simple

# 运行程序（方便调试）
run: $(TARGET)
	./$(TARGET)
