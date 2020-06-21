#include <stdint.h>

static inline uint8_t clz(uint64_t x);
static inline double sigma(double x);
static inline double tau(double x);

static inline void setDenseRegister(uint64_t m, uint8_t n, char *regs);
static inline uint64_t getDenseRegister(uint64_t m, char * regs);

void printByte(char a);
void setMemoryErrorMsg(uint64_t bytes);
uint8_t isValidIndex(uint64_t index, uint64_t size);
