#!/bin/bash
set -e

echo "=== Linux Driver Evaluation Setup Script ==="

# Update package list
sudo apt-get update -y
sudo apt-get upgrade -y

echo "[1/6] Installing build tools..."
sudo apt-get install -y build-essential make git pkg-config

echo "[2/6] Installing kernel headers for current kernel..."
# Try installing headers, fallback to generic if uname -r not available in repos
if sudo apt-get install -y linux-headers-$(uname -r); then
    echo "Kernel headers installed for $(uname -r)"
else
    echo "Specific kernel headers not found, installing generic headers..."
    sudo apt-get install -y linux-headers-generic
fi

echo "[3/6] Installing static analysis tools..."
sudo apt-get install -y sparse clang-format clang-tidy cppcheck

echo "[4/6] Installing debugging tools..."
sudo apt-get install -y gdb

echo "[5/6] Installing kernel module utilities..."
sudo apt-get install -y kmod

echo "[6/6] Cleaning up..."
sudo apt-get autoremove -y
sudo apt-get clean

echo "=== Setup Complete! ==="
echo "Tools installed:"
echo " - build-essential, make, git"
echo " - kernel headers"
echo " - sparse, clang-format, clang-tidy, cppcheck"
echo " - gdb"
echo " - kmod (insmod, rmmod, lsmod, modinfo)"

echo ""
echo "Next steps:"
echo "1. Try compiling a sample driver with 'make'"
echo "2. If Codespaces allows, test loading with 'sudo insmod mydriver.ko'"
echo "3. Run your evaluation scripts on test drivers"
