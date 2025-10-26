.PHONY: all install clean test test-basic help

# Default target
all: install test

# Install dependencies
install:
	@echo "Installing Bun dependencies..."
	@echo "Checking for Bun..."
	@command -v bun >/dev/null 2>&1 || { echo "Error: Bun is not installed. Please install Bun from https://bun.sh"; exit 1; }
	@echo "Building TypeScript package..."
	@cd ../../typescript && npm install && npm run build
	@echo "Installing local dependencies..."
	@bun install
	@echo "✓ Installation complete"

# Run all tests
test: test-basic
	@echo ""
	@echo "✓ All Bun example tests passed!"

# Run basic encoding test
test-basic:
	@echo ""
	@echo "Running basic encoding test..."
	@bun run test:basic

# Clean up
clean:
	@echo "Cleaning up..."
	@rm -rf node_modules
	@rm -f bun.lockb
	@rm -rf output
	@echo "✓ Cleanup complete"

# Help
help:
	@echo "Available targets:"
	@echo "  all        - Install dependencies and run all tests (default)"
	@echo "  install    - Install dependencies"
	@echo "  test       - Run all tests"
	@echo "  test-basic - Run basic encoding test"
	@echo "  clean      - Remove installed dependencies and outputs"
	@echo "  help       - Show this help message"
