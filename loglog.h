#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <math.h>
#include "murmur3.h"

double hyperLogLog(FILE * file, uint32_t k, uint32_t seed);

uint32_t leadingZeroCount(uint32_t x);

uint32_t ones(uint32_t x);
