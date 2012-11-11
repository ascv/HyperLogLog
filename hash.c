#include "loglog.h"

uint32_t leadingZeroCount(uint32_t x) {
  x |= (x >> 1);
  x |= (x >> 2);
  x |= (x >> 4);
  x |= (x >> 8);
  x |= (x >> 16);
  return (32 - ones(x));
}

uint32_t ones(uint32_t x) {
  x -= ((x >> 1) & 0x55555555);
  x = (((x >> 2) & 0x33333333) + (x & 0x33333333));
  x = (((x >> 4) + x) & 0x0F0F0F0F);
  x += (x >> 8);
  x += (x >> 16);
  return(x & 0x0000003F);
}

uint32_t jenkinsHash(const void *data, size_t len) {
  char * key = (char *)data;
  uint32_t hash, i;
  for(hash = i = 0; i < len; ++i) {
    hash += key[i];
    hash += (hash << 10);
    hash ^= (hash >> 6);
  }
  
  hash += (hash << 3);
  hash ^= (hash >> 11);
  hash += (hash << 15);
  return hash;
}

uint32_t hammingDistance(uint32_t x) {

  uint32_t dist = 0;
  uint32_t val =   x ^ 0xFFFFFFFF;
  while(val) {
    ++dist; 
    val &= val - 1;
  }
 
  return dist;
}
