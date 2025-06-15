CC = g++
CFLAGS = -Wall -Wextra -std=c++17 -Iinclude -g
LDFLAGS = -lws2_32  # Добавлена библиотека Winsock

SRC_DIR = ./src
BUILD_DIR = ./build
BIN_DIR = ./bin

# Исходные файлы
SRCS_SERVER = $(wildcard $(SRC_DIR)/server/*.cpp)
SRCS_CLIENT = $(wildcard $(SRC_DIR)/client/*.cpp)

# Объектные файлы
OBJS_CLIENT = $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRCS_CLIENT))
OBJS_SERVER = $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRCS_SERVER))

TARGET_SERVER = $(BIN_DIR)/server
TARGET_CLIENT = $(BIN_DIR)/client

all: create_dirs $(TARGET_SERVER) $(TARGET_CLIENT)

# Создание директорий
create_dirs:
	@mkdir -p $(BUILD_DIR)/client
	@mkdir -p $(BUILD_DIR)/server
	@mkdir -p $(BIN_DIR)

# Сборка исполняемого файла
$(TARGET_SERVER): $(OBJS_SERVER)
	$(CC) $^ -o $@ $(LDFLAGS)

$(TARGET_CLIENT): $(OBJS_CLIENT)
	$(CC) $^ -o $@ $(LDFLAGS)

# Правило для объектных файлов
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CC) $(CFLAGS) -c $< -o $@

# Очистка
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all create_dirs clean