#include "loglog.h"

uint32_t qhashmurmur3_32(const void *data, size_t nbytes) {
  if (data == NULL || nbytes == 0) return 0;

  const uint32_t c1 = 0xcc9e2d51;
  const uint32_t c2 = 0x1b873593;
 
  const int nblocks = nbytes / 4;
  const uint32_t *blocks = (const uint32_t *)(data);
  const uint8_t *tail = (const uint8_t *)(data + (nblocks * 4));
 
  uint32_t h = 0;
 
  int i;
  uint32_t k;
  for (i = 0; i < nblocks; i++) {
    k = blocks[i];
 
    k *= c1;
    k = (k << 15) | (k >> (32 - 15));
    k *= c2;
 
    h ^= k;
    h = (h << 13) | (h >> (32 - 13));
    h = (h * 5) + 0xe6546b64;
  }

  k = 0;
  switch (nbytes & 3) {
    case 3:
      k ^= tail[2] << 16;
    case 2:
      k ^= tail[1] << 8;
    case 1:
      k ^= tail[0];
      k *= c1;
      k = (k << 13) | (k >> (32 - 15));
      k *= c2;
      h ^= k;
  };

  h ^= nbytes;
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;
 
  return h;
}


uint32_t qhashfnv1_32(const void *data, size_t nbytes) {
  if (data == NULL || nbytes == 0) return 0;
 
  unsigned char *dp;
  uint32_t h = 0x811C9DC5;
 
  for (dp = (unsigned char *)data; *dp && nbytes > 0; dp++, nbytes--) {
#ifdef __GNUC__
    h += (h<<1) + (h<<4) + (h<<7) + (h<<8) + (h<<24);
#else
    h *= 0x01000193;
#endif
    h ^= *dp;
  }
  return h;
}

uint32_t lzc(uint32_t x) {
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

uint32_t jenkins_one_at_a_time_hash(const void *data, size_t len) {
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

uint32_t hamming_distance(uint32_t x) {

  uint32_t dist = 0;
  uint32_t val =   x ^ 0xFFFFFFFF;
  while(val) {
    ++dist; 
    val &= val - 1;
  }
 
  return dist;
}
