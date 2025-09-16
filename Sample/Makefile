# # Enhanced Makefile for WSL Driver Evaluation System
# CC = gcc
# CFLAGS = -Wall -Wextra -std=c99 -Wpedantic
# KERNEL_HEADERS = ./fake_kernel_headers

# # Different levels of checking
# check-basic:
# 	@echo "=== Basic Compilation Check ==="
# 	$(CC) $(CFLAGS) -I$(KERNEL_HEADERS) -c hello_driver.c -o hello_driver.o
# 	@echo "✅ Basic compilation successful"

# check-strict:
# 	@echo "=== Strict Compilation Check ==="
# 	$(CC) $(CFLAGS) -Werror -I$(KERNEL_HEADERS) -c hello_driver.c -o hello_driver_strict.o
# 	@echo "✅ Strict compilation successful (no warnings)"

# check-syntax-only:
# 	@echo "=== Syntax Only Check ==="
# 	$(CC) $(CFLAGS) -fsyntax-only -I$(KERNEL_HEADERS) hello_driver.c
# 	@echo "✅ Syntax check passed"

# analyze:
# 	@echo "=== Code Analysis ==="
# 	@echo "Checking for required components..."
# 	@grep -q "module_init" hello_driver.c && echo "✅ module_init found" || echo "❌ module_init missing"
# 	@grep -q "module_exit" hello_driver.c && echo "✅ module_exit found" || echo "❌ module_exit missing"
# 	@grep -q "MODULE_LICENSE" hello_driver.c && echo "✅ MODULE_LICENSE found" || echo "❌ MODULE_LICENSE missing"
# 	@grep -q "__init.*init" hello_driver.c && echo "✅ Init function found" || echo "❌ Init function missing"
# 	@grep -q "__exit.*exit" hello_driver.c && echo "✅ Exit function found" || echo "❌ Exit function missing"
# 	@grep -q "#include.*linux" hello_driver.c && echo "✅ Linux headers included" || echo "❌ Linux headers missing"

# count-lines:
# 	@echo "=== Code Metrics ==="
# 	@echo "Total lines: $$(wc -l < hello_driver.c)"
# 	@echo "Code lines (non-empty): $$(grep -v '^$$' hello_driver.c | wc -l)"
# 	@echo "Comment lines: $$(grep -c '^[[:space:]]*\*\|^[[:space:]]*/\*\|^[[:space:]]*///' hello_driver.c)"

# # Full evaluation (like your assignment system would do)
# evaluate:
# 	@echo "🔍 DRIVER EVALUATION SYSTEM"
# 	@echo "=========================="
# 	$(MAKE) setup-headers
# 	$(MAKE) analyze
# 	$(MAKE) count-lines
# 	$(MAKE) check-syntax-only
# 	$(MAKE) check-basic
# 	@echo "=========================="
# 	@echo "✅ Evaluation Complete!"

# clean:
# 	rm -f *.o
# 	rm -rf $(KERNEL_HEADERS)

# # Setup minimal headers for WSL
# setup-headers:
# 	@mkdir -p $(KERNEL_HEADERS)/linux
# 	@echo "#ifndef LINUX_INIT_H" > $(KERNEL_HEADERS)/linux/init.h
# 	@echo "#define LINUX_INIT_H" >> $(KERNEL_HEADERS)/linux/init.h
# 	@echo "#define __init" >> $(KERNEL_HEADERS)/linux/init.h
# 	@echo "#define __exit" >> $(KERNEL_HEADERS)/linux/init.h
# 	@echo "#endif" >> $(KERNEL_HEADERS)/linux/init.h
	
# 	@echo "#ifndef LINUX_MODULE_H" > $(KERNEL_HEADERS)/linux/module.h
# 	@echo "#define LINUX_MODULE_H" >> $(KERNEL_HEADERS)/linux/module.h
# 	@echo "#define MODULE_LICENSE(x)" >> $(KERNEL_HEADERS)/linux/module.h
# 	@echo "#define MODULE_AUTHOR(x)" >> $(KERNEL_HEADERS)/linux/module.h
# 	@echo "#define MODULE_DESCRIPTION(x)" >> $(KERNEL_HEADERS)/linux/module.h
# 	@echo "#define MODULE_VERSION(x)" >> $(KERNEL_HEADERS)/linux/module.h
# 	@echo "#define module_init(x)" >> $(KERNEL_HEADERS)/linux/module.h
# 	@echo "#define module_exit(x)" >> $(KERNEL_HEADERS)/linux/module.h
# 	@echo "#endif" >> $(KERNEL_HEADERS)/linux/module.h
	
# 	@echo "#ifndef LINUX_KERNEL_H" > $(KERNEL_HEADERS)/linux/kernel.h
# 	@echo "#define LINUX_KERNEL_H" >> $(KERNEL_HEADERS)/linux/kernel.h
# 	@echo "#define KERN_INFO" >> $(KERNEL_HEADERS)/linux/kernel.h
# 	@echo "int printk(const char *fmt, ...);" >> $(KERNEL_HEADERS)/linux/kernel.h
# 	@echo "#endif" >> $(KERNEL_HEADERS)/linux/kernel.h

# .PHONY: check-basic check-strict check-syntax-only analyze count-lines evaluate clean setup-headers

# Standard kernel module Makefile
obj-m += hello_driver.o

KDIR := /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

.PHONY: all clean