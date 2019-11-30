#include <stdint.h>

static inline uint8_t clz(uint64_t x);
static inline double sigma(double x);
static inline double tau(double x);

static inline void setReg(uint64_t m, uint8_t n, char *regs);
static inline uint64_t getReg(uint64_t m, char * regs);

void printByte(char a);
