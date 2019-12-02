#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "a.h"

/* ---------------------------- Test program ------------------------------- */

int main() {
    uint64_t N = 100000;
    char *regs = (char *)calloc(N, sizeof(char));
    uint64_t m;
    uint8_t regVal;

    for (m = 1; m < N; m++) {
        //uint8_t val = (uint8_t) m % 64;
        uint64_t val = rand() % 64;

        setReg(m, (uint8_t)val, regs);
        regVal = getReg(m, regs);

        if (regVal != val) {
            printf("Set m=%u ", (uint8_t)val);
            printf("got %u\n", regVal);
        }
    }

    printf("\n");
    //printRegs(regs);
    uint64_t X = 0;
    uint64_t nBits = 6*X + 6;
    uint64_t bytePos = nBits/8 - 1;
    printf("nBits: %lu bytePos: %lu\n", nBits, bytePos);
}

/* ---------------------------- Get and set ------------------------------- */


static inline uint8_t getReg(uint64_t m, char * regs)
{
    uint8_t reg;
    uint64_t nBits = 6*m + 6;
    uint64_t bytePos = nBits/8 - 1;

    uint8_t leftByte = regs[bytePos];
    uint8_t rightByte = regs[bytePos + 1];
    uint8_t nrb = (uint8_t) (nBits % 8);

    leftByte <<= nrb; /* Get the bits from the left byte */
    rightByte >>= (8 - nrb); /* Get the bits from the right byte */
    reg = leftByte | rightByte; /* OR the result to get the register */
    reg &= 63; /* Remove the higher order bits */

    return reg;
}

static inline void setReg(uint64_t m, uint8_t n, char *regs)
{
    uint64_t nBits = 6*m + 6;
    uint64_t bytePos = nBits/8 - 1;

    uint8_t nrb = (uint8_t) (nBits % 8);
    uint8_t nlb = 6 - nrb;
    uint8_t leftByte = regs[bytePos];
    uint8_t rightByte = regs[bytePos + 1];

    leftByte = (leftByte >> nlb) << nlb; /* Zero the left bits */
    rightByte = (rightByte << nrb) >> nrb; /* Zero the right bits */
    leftByte |= (n >> nrb); /* Set the new left bits */
    rightByte |= (n << (8 - nrb)); /* Set the new right bits */

    regs[bytePos] = leftByte;
    regs[bytePos + 1] = rightByte;
}

/* ---------------------------- Helpers ------------------------------- */

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
