void printByte(char a);
void printRegs(char * regs);

static inline uint8_t getReg(uint64_t m, char * regs);
static inline void setReg(uint64_t m, uint8_t n, char *regs);
