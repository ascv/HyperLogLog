#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "a.h"

/*
 * 0 --> last 6 bits of byte
 * 2 --> last 4 bits of byte, 2 bits of next byte
 * 4 --> last 2 bits of byte, 4 bits of next byte
 * 6 --> 6 bits of next byte
 */
int main() {
    char *regs = (char *)calloc(32, sizeof(char));
    uint8_t m;
    uint8_t regVal;

    for (m = 1; m < 31; m++) {
        printf("Setting m=%u ", 2*m);
        setReg(m, 2*m, regs);
        regVal = getReg(m, regs);
        printf("got %u\n", regVal);
    }

    printf("\n");
    printRegs(regs);

    for (m = 1; m < 31; m++) {
        printf("Setting m=%u ", m);
        setReg(m, m, regs);
        regVal = getReg(m, regs);
        printf("got %u\n", regVal);
    }

    printf("\n");
    printRegs(regs);
}

uint8_t updateReg(uint8_t m, uint8_t n, char * regs)
{
    return getSet(m, 1, regs, n);
}

uint8_t getReg(uint8_t m, char * regs) {
    return getSet(m, 0, regs, 0);
}

/*
 * m register
 * updateFlag 1 to update the register
 * regs registers
 * n update value
 */
uint8_t getSet(uint8_t m, uint8_t updateFlag, char * regs, uint8_t n)
{

    /*
     * Register encoding  TODO JOSH: fix byte print statement
     * -----------------
     *
     * Register values store the maximum positions of the first set bit. Since
     * the maximum first set bit position is 63 we can save space by using 6
     * bits to store the register values (instead of 1 byte). The encoding
     * scheme is diagrammed below:
     *
     *          b0        b1        b3        b4
     *          /         /         /         /
     *     +-------------------+---------+---------+
     *     |0000 0011|1111 0000|0110 1110|1111 1011|
     *     +-------------------+---------+---------+
     *      |_____||_____| |_____||_____| |_____|
     *         |      |       |      |       |
     *       offset   m1      m2     m3     m4
     *
     *      b = bytes, m = registers
     *
     *
     * The first six bits in b0 are an unused offset. With the exception
     * of byte aligned registers, registers will have bits in consecutive
     * bytes. For example, the register m2 has bits in b1 and b2.
     *
     * Getting a register
     * ------------------
     *
     * To get a particular register we determine the containing bytes, isolate
     * the register bits, put them together, and then return the result.
     *
     * For example, suppose we want to get the value of the second register m2
     * (e.g. m=2). First we determine the bytes containing m2:
     *
     *     left byte  = (6*m + 6)/8 - 1
     *                = 1
     *
     *     right byte = left byte + 1
     *                = 2
     *
     * So the containing bytes are b1 and b2. Next, we determine the bits of
     * m in each byte:
     *
     *     right bits = (6*m + 6) % 8
     *                = 2
     *
     *     left bits  = 6 - right bits
     *                = 4
     *
     * This result is diagrammed below:
     *
     *
     *     +---------+---------+
     *     |1111 0000|0110 1110|
     *     +---------+---------+
     *           ^^^^ ^^
     *           /      \
     *       left bits  right bits
     *
     *      m2 = 000001
     *
     * To construct m2 from the 4 bits in b1 and the 2 bits in b0 we
     *
     */

    uint8_t reg;
    uint8_t nBits = 6*m + 6;
    uint8_t bytePos = nBits/8 - 1;
    uint8_t mLeftByte = regs[bytePos];
    uint8_t mRightByte = regs[bytePos + 1];
    uint8_t nRightBits = nBits % 8;

    mLeftByte <<= nRightBits;  /* Get the bits from the left byte */
    mRightByte >>= (8 - nRightBits);  /* Get the bits from the right byte */
    reg = mLeftByte | mRightByte; /* OR the result to get the register */
    reg &= 63;  /* Remove the higher order bits */

    return reg;
}

void setReg(uint8_t m, uint8_t n, char *regs)
{
    uint8_t nBits = 6*m + 6;
    uint8_t bytePos = nBits/8 - 1;
    uint8_t nRightBits = nBits % 8;
    uint8_t nLeftBits = 6 - nRightBits;
    uint8_t leftByte = regs[bytePos];
    uint8_t rightByte = regs[bytePos + 1];

    /* Zero out the left bits */
    leftByte >>= nLeftBits;
    leftByte <<= nLeftBits;

    /* Zero out the right bits */
    rightByte <<= nRightBits;
    rightByte >>= nRightBits;

    /* Set the new bytes */
    regs[bytePos] = (leftByte | (n >> nRightBits));
    regs[bytePos + 1] = (rightByte | (n << (8- nRightBits)));
}


void printByte(char a)
{
    int i;
    for (i = 0; i < 8; i++) {
        printf("%d", !!((a << i) & 0x80));
    }
}

void printRegs(char * regs)
{
    uint8_t i;
    uint8_t N = 16;
    /* Print base 10 values */
    for (i = 0; i < N; i++) {
        printf("[%u]->%u ", i, regs[i]);
    }
    printf("\n\n");

    /* Print byte indices */
    for (i = 0; i < N; i++) {
        printf("    %u    ", i);
    }
    for (i = 10; i < N; i++) {
        printf("    %u   ", i);
    }
    printf("\n");

    /* Print binary values */
    for (i = 0; i < N; i++) {
        printf("|");
        printByte(regs[i]);
    }
    printf("|\n");
}
