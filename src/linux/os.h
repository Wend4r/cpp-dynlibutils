// This file is used to not include os specific functions that might break other projects
// You should use it in sources
#pragma once

#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <link.h>
#include <sys/mman.h>
#include <sys/stat.h>