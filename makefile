BUILD_DIR = build
BINARY_NAME = cascade

.PHONY: all build run clean

all: build run

build:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. && make

run:
	@if [ -f $(BUILD_DIR)/$(BINARY_NAME) ]; then \
		./$(BUILD_DIR)/$(BINARY_NAME); \
	else \
		echo "Binary not found. Run 'make build' first."; \
	fi

clean:
	@rm -rf $(BUILD_DIR)
	@echo "Build directory cleaned."