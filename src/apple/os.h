// This file is used to not include os specific functions that might break other projects
// You should use it in sources
#pragma once

#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <mach-o/nlist.h>
#include <sys/mman.h>
#include <sys/stat.h>
