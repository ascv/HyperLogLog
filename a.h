void printByte(char a);
void printRegs(char * regs);

uint8_t updateReg(uint8_t m, uint8_t n, char * regs);
uint8_t getReg(uint8_t m, char * regs);
uint8_t getSet(uint8_t m, uint8_t updateFlag, char * regs, uint8_t n);
void setReg(uint64_t m, uint8_t n, char *regs);
