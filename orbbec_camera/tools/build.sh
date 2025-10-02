#!/bin/bash
# Simple build script for Orbbec CLI tool

set -e  # Exit on error

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}   Building Orbbec CLI Tool${NC}"
echo -e "${BLUE}========================================${NC}"

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# Create build directory
echo -e "${YELLOW}Creating build directory...${NC}"
rm -rf build_tools
mkdir -p build_tools
cd build_tools

# Configure with CMake
echo -e "${YELLOW}Configuring with CMake...${NC}"
cmake ..

# Build
echo -e "${YELLOW}Building...${NC}"
make -j$(nproc)

# Install (copies to tools directory)
echo -e "${YELLOW}Installing...${NC}"
make install

# Success
cd ..
echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}   Build completed successfully!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo -e "Executable created:"
echo -e "  ${GREEN}✓${NC} ./orbbec_cli"
echo ""
echo -e "Run with:"
echo -e "  ${BLUE}./orbbec_cli${NC}"
echo ""
