CFLAGS += -Wall -ffreestanding -fno-builtin -fno-stack-protector -fno-zero-initialized-in-bss -nostdinc
INCLUDES=-I$(PROJECT_ROOT)/src/programs/libprog -I$(PROJECT_ROOT)/src/programs/libs/CKeyedBits/include -I$(PROJECT_ROOT)/libs/anlock/src -I$(PROJECT_ROOT)/src/programs/libprog/libc -I$(PROJECT_ROOT)/libs/anmem/libs/analloc/src -I$(PROJECT_ROOT)/src/programs/libprog/base -I$(PROJECT_ROOT)/src/programs/libs/anmalloc/include -I$(PROJECT_ROOT)/src/programs/libprog/bindings/include
CPPINCLUDES=-I$(PROJECT_ROOT)/src/programs/libcpp
