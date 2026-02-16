#define PY_SSIZE_T_CLEAN
#define HLL_VERSION "3.0.0"

#include <math.h>
#include <Python.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "hll.h"
#include "structmember.h"
#include "../lib/murmur2.h"

/* Packed sparse entry: bits 31..6 = register index, bits 5..0 = fsb value.
 * Supports p up to 26. Each entry is 4 bytes instead of 16. */
typedef uint32_t SparseEntry;

#define SPARSE_INDEX(e) ((uint32_t)(e) >> 6)
#define SPARSE_FSB(e)   ((uint8_t)((e) & 0x3F))
#define SPARSE_ENTRY(idx, fsb) (((uint32_t)(idx) << 6) | ((uint32_t)(fsb) & 0x3F))

typedef struct {
    PyObject_HEAD
    uint8_t* registers; /* Densely encoded registers */
    unsigned short p; /* 2^p = number of registers */
    uint64_t * histogram; /* Register histogram */
    uint64_t seed; /* MurmurHash64A seed */
    uint64_t size; /* Number of registers */
    uint64_t cache; /* Cached cardinality estimate */
    uint64_t added; /* Number of elements added */
    bool isCached; /* If the cache is up to date */
    bool isSparse; /* If sparse encoding is currently in use */

    /* Fields used for sparse representation */
    SparseEntry* sparseRegisters; /* Sorted array of non-zero registers */
    SparseEntry* sparseBuffer; /* Temporary buffer of entries to be merged */
    uint64_t sparseCount; /* Number of entries in the sorted array */
    uint64_t sparseCapacity; /* Allocated capacity of the sorted array */
    uint64_t bufferSize; /* Number of elements in the temporary buffer */
    uint64_t maxBufferSize; /* Max number of elements for the temporary buffer */
    uint64_t maxListSize; /* Used to derive default maxBufferSize */
} HyperLogLog;


/* ========================== Dense representation ========================= */
/*
 * Since register values will never exceed 64 we store them using only 6 bits.
 * This encoding is diagrammed below:
 *
 *          b0        b1        b2        b3
 *          /         /         /         /
 *     +---------+---------+---------+---------+
 *     |0000 0011|1111 0011|0110 1110|1111 1011|
 *     +---------+---------+---------+---------+
 *      |_____||_____| |_____||_____| |_____|
 *         |      |       |      |       |
 *       offset   m1      m2     m3     m4
 *
 *      b = bytes, m = registers
 *
 * The first six bits in b0 are an unused offset. With the exception of byte
 * aligned registers (e.g. m4), registers will have bits in consecutive bytes.
 * For example, the register m2 has bits in b1 and b2. The higher order bits
 * of m2 are in b1 and the lower order bytes of m2 are in the b2.
 *
 * Getting a register
 * ------------------
 *
 * Suppose we want to get register m2 (e.g. m=2). First we determine the
 * indices of the enclosing bytes:
 *
 *     left byte  = (6*m + 6)/8 - 1                                         (1)
 *                = 1
 *
 *     right byte = left byte + 1                                           (2)
 *                = 2
 *
 * Next we compute the number of bits of m2 in each byte. The number of right
 * bits is:
 *
 *     rb = right bits                                                      (3)
 *        = (6*m + 6) % 8
 *        = 2
 *
 *     lb = left bits                                                       (4)
 *        = 6 - rb
 *        = 4
 *
 * This result is diagrammed below:
 *
 *         b1         b2
 *          \         /
 *     +---------+---------+
 *     |1111 0011|0110 1110|
 *     +---------+---------+
 *           ^^^^ ^^
 *           /      \
 *       left bits  right bits
 *
 *       m2 = "001101"
 *
 * Move the left bits into the higher order positions:
 *
 *     +---------+
 *     |1111 0011|   <-- b1
 *     |1100 1100|   <-- b1 << rb
 *     +---------+
 *
 * Move the right bits to the lower order position:
 *
 *     +---------+
 *     |0110 1110|   <-- b2
 *     |0000 0001|   <-- b2 >> (8 - rb)
 *     +---------+
 *
 * Bitwise OR the two bytes, b1 | b2:
 *
 *     +---------+
 *     |1100 1100|  <-- b1
 *     |0000 0001|  <-- b2
 *     |1100 1101|  <-- b1 | b2
 *     +---------+
 *
 * Finally use a mask to remove the bits not part of m2:
 *
 *     +---------+
 *     |1100 1101|  <-- b1 | b2
 *     |0011 1111|  <-- mask to isolate the register bits
 *     |0000 1101|  <-- m2 = b1 & mask
 *     +---------+
 *
 * Setting a register
 * ------------------
 *
 * Setting a register is similar to getting a register. We determine the
 * enclosing bytes using (1) and (2). Then the bits of each byte is
 * computed using (3) and (4). Continuing the previous example using register
 * m2, at this point we should have:
 *
 *         b1         b2
 *          \         /
 *     +---------+---------+
 *     |1111 0011|0110 1110|
 *     +---------+---------+
 *           ^^^^ ^^
 *           /      \
 *       left bits  right bits
 *
 *       lb = 4, rb = 2
 *       m2 = "001101"
 *
 * Let N be the value we want to set. Suppose we want to set m2 to 7 (N=7). We
 * start by zeroing out the left bits of m in b1 and the rights bits of m in
 * b2:
 *
 *     +---------+
 *     |1111 0011|  <- b1
 *     |0011 1100|  <- b1 = b1 >> lb
 *     |1111 0000|  <- b1 = b1 << lb
 *     |0110 1110|  <- b2
 *     |1011 1000|  <- b2 = b2 << rb
 *     |0010 1110|  <- b2 = b2 >> rb
 *     +---------+
 *
 * Now that we have made space for m2, we need to set the new bits. We can get
 * new bits by simplying shifting N:
 *
 *      new right bits
 *            \
 *            vv
 *    +---------+
 *    |0000 0111|  <- N=7
 *    +---------+
 *       ^^ ^^
 *        \ /
 *    new left bits
 *
 *    nlb = new left bits
 *        = N >> rb
 *        = 7 >> 2
 *
 *    nrb = new right bits
 *        = N << (8 - rb)
 *        = 7 << 6
 *
 * We can now set the left byte b1 using bitwise OR:
 *
 *    +---------+
 *    |1111 0000|  <- b1
 *    |0000 0001|  <- nlb
 *    |1111 0001|  <- b1 | nlb
 *    +---------+
 *
 * Setting the right byte b2 using bitwise OR:
 *
 *    +---------+
 *    |0010 1110|  <- b2
 *    |1100 0000|  <- nrb
 *    |1110 1110|  <- b2 | nrb
 *    +---------+
 *
 * The bytes have been updated so we're done. The final result is shown
 * below:
 *
 *         b1         b2
 *          \         /
 *     +---------+---------+
 *     |1111 0001|1110 1110|
 *     +---------+---------+
 *           ^^^^ ^^
 *           /      \
 *       left bits  right bits
 *
 *       lb = 4, rb = 2
 *       m2 = "000111"
 */


/* Get register m. */
static inline uint64_t getDenseRegister(uint64_t m, uint8_t* regs)
{
    uint64_t nBits = 6*m + 6;
    uint64_t bytePos = nBits/8 - 1;
    uint8_t leftByte = regs[bytePos];
    uint8_t rightByte = regs[bytePos + 1];
    uint8_t nrb = (uint8_t) (nBits % 8);
    uint8_t reg;

    leftByte <<= nrb; /* Move left bits into high order spots */
    rightByte >>= (8 - nrb); /* Move rights bits into the low order spots */
    reg = leftByte | rightByte; /* OR the result to get the register */
    reg &= 63; /* Get rid of the 2 extra bits */

    return (uint64_t) reg;
}


/* Set register m to n. */
static inline void setDenseRegister(uint64_t m, uint8_t n, uint8_t* regs)
{
    uint64_t nBits = 6*m + 6;
    uint64_t bytePos = nBits/8 - 1;
    uint8_t nrb = (uint8_t) (nBits % 8);
    uint8_t nlb = 6 - nrb;
    uint8_t leftByte = regs[bytePos];
    uint8_t rightByte = regs[bytePos + 1];

    leftByte >>= nlb;
    leftByte <<= nlb;
    rightByte <<= nrb;
    rightByte >>= nrb;
    leftByte |= (n >> nrb); /* Set the new left bits */
    rightByte |= (n << (8 - nrb)); /* Set the new right bits */
    regs[bytePos] = leftByte;
    regs[bytePos + 1] = rightByte;
}


/* ========================== Sparse representation ======================== */
/*
 * When a HyperLogLog is created its register values are initialized to zero.
 * Because the registers share the same value it is inefficient to store
 * them individually. Instead only non-zero registers are stored in a sorted
 * dynamic array of packed 32-bit entries. Each entry encodes a register index
 * and its value into a single uint32_t:
 *
 *     +--------------------------------+
 *     | index (bits 31..6) | fsb (5..0)|
 *     +--------------------------------+
 *
 * For example the registers
 *
 *     +-+-+-+-+-+-+-+-+
 *     |0|3|0|0|1|1|0|2|
 *     +-+-+-+-+-+-+-+-+
 *
 * are represented with the following sorted array:
 *
 *     +-----+-----+-----+-----+
 *     | 1,3 | 4,1 | 5,1 | 7,2 |
 *     +-----+-----+-----+-----+
 *       [0]   [1]   [2]   [3]
 *
 * Because the array is sorted by index, individual registers can be looked
 * up using binary search in O(log n). The array grows dynamically, doubling
 * in capacity when more space is needed via realloc.
 *
 * To avoid an O(n) insertion into the sorted array every time add() is
 * called, a temporary buffer collects new entries. When the buffer is full
 * it is sorted and merged into the main array in one pass. Because both
 * the array and buffer are sorted this merge runs in O(n + k) time.
 *
 * Eventually the sparse array's allocated memory approaches that of the
 * dense representation. When the capacity would exceed the dense size in
 * bytes, the HyperLogLog switches to dense representation.
 */


/* Compares two packed sparse entries. Sorts ascending by index, then
 * descending by fsb so that the highest value comes first for duplicates. */
int compareEntries(const void* a, const void* b) {
    SparseEntry A = *(const SparseEntry*)a;
    SparseEntry B = *(const SparseEntry*)b;
    uint32_t idxA = SPARSE_INDEX(A);
    uint32_t idxB = SPARSE_INDEX(B);

    if (idxA != idxB) return (idxA < idxB) ? -1 : 1;

    /* Same index: descending by fsb so highest value comes first */
    uint8_t fsbA = SPARSE_FSB(A);
    uint8_t fsbB = SPARSE_FSB(B);
    if (fsbA > fsbB) return -1;
    if (fsbA < fsbB) return 1;
    return 0;
}


/* Merges the temporary buffer into the sorted register array. Both sequences
 * are sorted, so the merge runs in a single backwards pass. */
void flushRegisterBuffer(HyperLogLog* self)
{
    uint64_t i, j, w, bufCount, needed, newCap, dupes;

    if (self->bufferSize == 0) return;

    /* Sort buffer by index ascending, fsb descending for same index */
    qsort(self->sparseBuffer, self->bufferSize, sizeof(SparseEntry), compareEntries);

    /* Deduplicate buffer: keep only the first entry per index (highest fsb) */
    bufCount = 1;
    for (i = 1; i < self->bufferSize; i++) {
        if (SPARSE_INDEX(self->sparseBuffer[i]) != SPARSE_INDEX(self->sparseBuffer[bufCount - 1])) {
            self->sparseBuffer[bufCount] = self->sparseBuffer[i];
            bufCount++;
        }
    }

    /* Ensure capacity for the merge */
    needed = self->sparseCount + bufCount;
    if (needed > self->sparseCapacity) {
        newCap = self->sparseCapacity ? self->sparseCapacity : 32;
        while (newCap < needed) newCap *= 2;
        self->sparseRegisters = (SparseEntry*)realloc(
            self->sparseRegisters, newCap * sizeof(SparseEntry));
        self->sparseCapacity = newCap;
    }

    /* Backwards merge: walk both sorted sequences from the end and write
     * into the tail of sparseRegisters. This is safe because the write
     * position is always >= the read position in the existing array. */
    i = self->sparseCount; /* read cursor for existing entries */
    j = bufCount;          /* read cursor for buffer entries */
    w = needed;            /* write cursor */
    dupes = 0;

    while (i > 0 && j > 0) {
        uint32_t regIdx = SPARSE_INDEX(self->sparseRegisters[i - 1]);
        uint32_t bufIdx = SPARSE_INDEX(self->sparseBuffer[j - 1]);

        if (regIdx > bufIdx) {
            self->sparseRegisters[--w] = self->sparseRegisters[--i];
        } else if (regIdx < bufIdx) {
            /* New entry from buffer */
            self->sparseRegisters[--w] = self->sparseBuffer[--j];
            self->histogram[0]--;
            self->histogram[SPARSE_FSB(self->sparseRegisters[w])]++;
        } else {
            /* Duplicate index: take the max fsb */
            --i; --j; --w;
            uint8_t bufFsb = SPARSE_FSB(self->sparseBuffer[j]);
            uint8_t regFsb = SPARSE_FSB(self->sparseRegisters[i]);
            if (bufFsb > regFsb) {
                self->histogram[regFsb]--;
                self->histogram[bufFsb]++;
                self->sparseRegisters[w] = SPARSE_ENTRY(regIdx, bufFsb);
            } else {
                self->sparseRegisters[w] = self->sparseRegisters[i];
            }
            dupes++;
        }
    }

    /* Copy remaining buffer entries */
    while (j > 0) {
        self->sparseRegisters[--w] = self->sparseBuffer[--j];
        self->histogram[0]--;
        self->histogram[SPARSE_FSB(self->sparseRegisters[w])]++;
    }

    /* Close the gap left by duplicates. Untouched entries sit at 0..i-1,
     * merged entries sit at w..needed-1. Shift them together. */
    if (w > i) {
        memmove(self->sparseRegisters + i, self->sparseRegisters + w,
                (needed - w) * sizeof(SparseEntry));
    }

    self->sparseCount = needed - dupes;
    self->bufferSize = 0;
}


/* Transforms a HyperLogLog from sparse to dense representation.
 * Returns 0 on success, -1 on allocation failure (sets MemoryError). */
int transformToDense(HyperLogLog* self) {
    uint64_t bytes = (self->size*6)/8 + 1;
    self->registers = (uint8_t*)calloc(bytes, sizeof(uint8_t));

    if (self->registers == NULL) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Failed to allocate %lu bytes.", bytes);
        PyErr_SetString(PyExc_MemoryError, msg);
        return -1;
    }

    flushRegisterBuffer(self);

    for (uint64_t i = 0; i < self->sparseCount; i++) {
        setDenseRegister(SPARSE_INDEX(self->sparseRegisters[i]),
                         SPARSE_FSB(self->sparseRegisters[i]),
                         self->registers);
    }

    free(self->sparseRegisters);
    self->sparseRegisters = NULL;
    self->sparseCount = 0;
    self->sparseCapacity = 0;

    free(self->sparseBuffer);
    self->sparseBuffer = NULL;

    self->isSparse = 0;

    return 0;
}


/* Gets the register value at the specified index using binary search. */
static inline uint64_t
getSparseRegister(HyperLogLog* self, uint64_t index)
{
    uint64_t lo, hi, mid;

    if (self->bufferSize > 0) {
        flushRegisterBuffer(self);
    }

    lo = 0;
    hi = self->sparseCount;

    while (lo < hi) {
        mid = lo + (hi - lo) / 2;
        uint32_t midIdx = SPARSE_INDEX(self->sparseRegisters[mid]);
        if (midIdx < index) {
            lo = mid + 1;
        } else if (midIdx > index) {
            hi = mid;
        } else {
            return SPARSE_FSB(self->sparseRegisters[mid]);
        }
    }

    return 0;
}


/* Sets a sparse register. This function does not set the register immediately
 * but instead adds it to the temporary buffer. Register updates will occur
 * when the buffer is next cleared. */
static inline void setSparseRegister(HyperLogLog* self, uint64_t index, uint8_t fsb)
{
    /* Add an element to the buffer if there is room */
    if (self->bufferSize < self->maxBufferSize) {
        self->sparseBuffer[self->bufferSize] = SPARSE_ENTRY(index, fsb);
        self->bufferSize++;
    }

    /* Otherwise flush the buffer and then add */
    else {
        flushRegisterBuffer(self);
        self->bufferSize = 1;
        self->sparseBuffer[0] = SPARSE_ENTRY(index, fsb);
    }
}


/* ====================== HyperLogLog object methods ======================= */


/* Set a HyperLogLog register. This is a convenience function intended to make
 * register updates representation agnostic. */
static inline int setRegister(HyperLogLog* self, uint64_t index, uint8_t newFsb) {
    self->added++; /* Increment method call counter */

    if (self->isSparse) {
        setSparseRegister(self, index, newFsb);

        /* Switch to dense when the sparse array allocation exceeds
         * the dense representation size. This triggers right after a
         * buffer flush doubles the capacity past the threshold. */
        uint64_t denseBytes = (self->size * 6) / 8 + 1;
        if (self->sparseCapacity * sizeof(SparseEntry) >= denseBytes) {
            if (transformToDense(self) < 0) {
                return -1;
            }
        }

        self->isCached = 0;
    } else {
        uint64_t fsb = getDenseRegister(index, self->registers);

        if (newFsb > fsb) {
            setDenseRegister(index, (uint8_t)newFsb, self->registers);
            self->histogram[newFsb] += 1; /* Increment the new count */
            self->isCached = 0;

            if (self->histogram[fsb] == 0) {
                self->histogram[0] -= 1;
            } else {
                self->histogram[fsb] -= 1;
            }

            return 1;
        }
    }

    return 0;
}


/* Gets the a register value by index */
static PyObject* HyperLogLog_get_register(HyperLogLog* self, PyObject* args)
{
    unsigned long index;
    uint64_t fsb;

    if (!PyArg_ParseTuple(args, "k", &index)) return NULL;
    if (!isValidIndex(index, self->size)) return NULL;

    if (self->isSparse) {
        fsb = getSparseRegister(self, index);
    } else {
        fsb = getDenseRegister(index, self->registers);
    }

    return Py_BuildValue("k", fsb);
}


/* Returns all register values as a bytes object of length 2^p.
 * Each byte is one register value (0-63). */
static PyObject* HyperLogLog_registers(HyperLogLog* self)
{
    uint64_t size = self->size;
    PyObject* result = PyBytes_FromStringAndSize(NULL, (Py_ssize_t)size);
    if (result == NULL) return NULL;

    uint8_t* buf = (uint8_t*)PyBytes_AS_STRING(result);

    if (self->isSparse) {
        if (self->bufferSize > 0) {
            flushRegisterBuffer(self);
        }

        /* Initialize all registers to zero */
        memset(buf, 0, size);

        /* Walk the sorted array and fill in non-zero registers */
        for (uint64_t i = 0; i < self->sparseCount; i++) {
            buf[SPARSE_INDEX(self->sparseRegisters[i])] = SPARSE_FSB(self->sparseRegisters[i]);
        }
    } else {
        /* Unpack 6-bit dense registers */
        for (uint64_t i = 0; i < size; i++) {
            buf[i] = (uint8_t)getDenseRegister(i, self->registers);
        }
    }

    return result;
}


/* Gets a dictionary of internal attributes and their values */
static PyObject* HyperLogLog__get_meta(HyperLogLog* self, PyObject* args)
{
    char version[8];
    sprintf(version, "%u.%u.%u", PY_MAJOR_VERSION, PY_MINOR_VERSION, PY_MICRO_VERSION);

    return Py_BuildValue("{s:k,s:k,s:k,s:k,s:k,s:i,s:i,s:k,s:k,s:s,s:s}",
        "added", self->added,
        "list_size", self->sparseCount,
        "sparse_capacity", self->sparseCapacity,
        "buffer_size", self->bufferSize,
        "cache", self->cache,
        "is_cached", self->isCached,
        "is_sparse", self->isSparse,
        "max_list_size", self->maxListSize,
        "max_buffer_size", self->maxBufferSize,
        "py_version", version,
        "hll_version", HLL_VERSION
    );
}


/* Gets a histogram of first set bit positions as a list of ints. */
static PyObject* HyperLogLog__histogram(HyperLogLog* self)
{
    PyObject* histogram = PyList_New(65);

    for (int i = 0; i < 65; i++) {
        PyObject* count = Py_BuildValue("i", self->histogram[i]);
        PyList_SetItem(histogram, i, count);
    }

    return histogram;
}


static void HyperLogLog_dealloc(HyperLogLog* self)
{
    free(self->histogram);
    free(self->registers);

    if (self->isSparse) {
        free(self->sparseRegisters);
        free(self->sparseBuffer);
    }

    Py_TYPE(self)->tp_free((PyObject*) self);
}


/* Add an element. */
static PyObject* HyperLogLog_add(HyperLogLog* self, PyObject* args)
{
    const char* data;
    Py_ssize_t dataLen;
    uint64_t hash, index, newFsb;

    if (!PyArg_ParseTuple(args, "s#", &data, &dataLen)) return NULL;
    hash = MurmurHash64A((void*)data, dataLen, self->seed);

    index = (hash >> (64 - self->p)); /* Use the first p bits as an index */
    newFsb = hash << self->p; /* Remove the first p bits */
    newFsb = clz(newFsb) + 1; /* Find the first set bit in the remaining bits */
    int updated = setRegister(self, index, (uint8_t)newFsb);

    if (updated < 0) return NULL;

    if (updated) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
};


/* Add a range of sequential 8-byte little-endian integers [start, start+count).
 * This avoids Python-to-C overhead for bulk insertions. */
static PyObject* HyperLogLog_add_range(HyperLogLog* self, PyObject* args)
{
    uint64_t start, count;

    if (!PyArg_ParseTuple(args, "KK", &start, &count)) return NULL;

    for (uint64_t i = 0; i < count; i++) {
        uint64_t val = start + i;
        uint64_t hash = MurmurHash64A((void*)&val, 8, self->seed);
        uint64_t index = (hash >> (64 - self->p));
        uint64_t newFsb = hash << self->p;
        newFsb = clz(newFsb) + 1;
        if (setRegister(self, index, (uint8_t)newFsb) < 0) return NULL;
    }

    Py_RETURN_NONE;
}


/* Get a cardinality estimate */
static PyObject* HyperLogLog_cardinality(HyperLogLog* self)
{
    if (self->isCached) {
        return Py_BuildValue("K", self->cache);
    } else if (self->isSparse && self->bufferSize > 0) {
        flushRegisterBuffer(self);
    }

    double alpha = 0.7213475;
    double m = (double)self->size;
    double z = m*tau((m - (double)self->histogram[self->p + 1])/m);

    uint64_t k;
    for (k = 64 - self->p; k >= 1; --k) {
        z += self->histogram[k];
        z *= 0.5;
    }

    z += m*sigma((double)self->histogram[0]/m);
    uint64_t estimate = (uint64_t)round(alpha*m*(m/z));

    self->cache = estimate;
    self->isCached = 1;

    return Py_BuildValue("K", estimate);
}


/* Get a Murmur64A hash of a string, buffer or bytes object. */
static PyObject* HyperLogLog_hash(HyperLogLog* self, PyObject* args)
{
    const char* data;
    Py_ssize_t dataLen;

    if (!PyArg_ParseTuple(args, "s#", &data, &dataLen)) return NULL;

    uint64_t hash = MurmurHash64A((void*) data, dataLen, self->seed);
    return Py_BuildValue("K", hash);
}


static int HyperLogLog_init(HyperLogLog* self, PyObject* args, PyObject* kwds)
{
    static char* kwlist[] = {"p", "seed", "sparse", "max_sparse_list_size", "max_sparse_buffer_size", NULL};
    uint64_t maxSparseListSize = 0;
    uint64_t maxSparseBufferSize = 0;
    int64_t sparse = 1;

    self->seed = 314;  /* Chosen arbitrarily */
    self->p = 12;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|iiikk", kwlist, &self->p, &self->seed, &sparse, &maxSparseListSize, &maxSparseBufferSize)) {
        return -1;
    }

    if (self->p < 2 || self->p > 63) {
        char* msg = "p is out of range";
        PyErr_SetString(PyExc_ValueError, msg);
        return -1;
    }

    self->added = 0;
    self->cache = 0;
    self->isCached = 0;
    self->sparseCount = 0;
    self->sparseCapacity = 0;
    self->size = 1UL << self->p;
    self->histogram = (uint64_t*)calloc(65, sizeof(uint64_t)); /* Keep a count of register values */
    self->histogram[0] = self->size; /* Set the zeroes count */
    self->registers = NULL;
    self->sparseRegisters = NULL;
    self->sparseBuffer = NULL;

    if (sparse) {
        self->isSparse = 1;

        if (maxSparseListSize > 0) {
            self->maxListSize = maxSparseListSize;
        } else {
            uint64_t defaultSize = self->size/4;
            uint64_t maxDefaultSize = 1 << 20;

            if (maxDefaultSize < defaultSize) {
                self->maxListSize = maxDefaultSize;
            } else if (defaultSize <= 4) { /* This shouldn't happen, but do something reasonable if it does */
                self->maxListSize = 2;
            } else {
                self->maxListSize = defaultSize;
            }
        }

        if (maxSparseBufferSize > 0) {
            self->maxBufferSize = maxSparseBufferSize;
        } else {
            uint64_t defaultSize = self->maxListSize/2;
            uint64_t maxDefaultSize = 200000;

            if (maxDefaultSize < defaultSize) {
                self->maxBufferSize = maxDefaultSize;
            } else {
                self->maxBufferSize = defaultSize;
            }
        }

        self->sparseBuffer = (SparseEntry*)malloc(sizeof(SparseEntry) * self->maxBufferSize);
    } else {
        uint64_t bytes = (self->size*6)/8 + 1;
        self->registers = (uint8_t*)calloc(bytes, sizeof(uint8_t));

        if (self->registers == NULL) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Failed to allocate %lu bytes. Use a smaller p.", bytes);
            PyErr_SetString(PyExc_MemoryError, msg);
            return -1;
        }
    }

    return 0;
}


/* Fast merge path when both sketches are sparse. Two-pointer walk over
 * sorted arrays: O(countA + countB) instead of O(2^p).
 * Returns 0 on success, -1 on error (sets Python exception). */
static int mergeSparse(HyperLogLog* self, HyperLogLog* other)
{
    flushRegisterBuffer(self);
    flushRegisterBuffer(other);

    uint64_t countA = self->sparseCount;
    uint64_t countB = other->sparseCount;
    SparseEntry* A = self->sparseRegisters;
    SparseEntry* B = other->sparseRegisters;

    /* Worst case: all entries are disjoint */
    uint64_t needed = countA + countB;
    if (needed > self->sparseCapacity) {
        uint64_t newCap = self->sparseCapacity ? self->sparseCapacity : 32;
        while (newCap < needed) newCap *= 2;
        self->sparseRegisters = (SparseEntry*)realloc(
            self->sparseRegisters, newCap * sizeof(SparseEntry));
        A = self->sparseRegisters;
        self->sparseCapacity = newCap;
    }

    /* Backwards merge into tail of sparseRegisters (same technique as
     * flushRegisterBuffer). Safe because write pos >= read pos. */
    uint64_t i = countA;  /* read cursor for self */
    uint64_t j = countB;  /* read cursor for other */
    uint64_t w = needed;  /* write cursor */
    uint64_t dupes = 0;

    while (i > 0 && j > 0) {
        uint32_t idxA = SPARSE_INDEX(A[i - 1]);
        uint32_t idxB = SPARSE_INDEX(B[j - 1]);

        if (idxA > idxB) {
            A[--w] = A[--i];
        } else if (idxA < idxB) {
            /* New entry from other */
            A[--w] = B[--j];
            self->histogram[0]--;
            self->histogram[SPARSE_FSB(A[w])]++;
        } else {
            /* Same index: take the max fsb */
            --i; --j; --w;
            uint8_t fsbA = SPARSE_FSB(A[i]);
            uint8_t fsbB = SPARSE_FSB(B[j]);
            if (fsbB > fsbA) {
                self->histogram[fsbA]--;
                self->histogram[fsbB]++;
                A[w] = SPARSE_ENTRY(idxA, fsbB);
            } else {
                A[w] = A[i];
            }
            dupes++;
        }
    }

    /* Copy remaining entries from other */
    while (j > 0) {
        A[--w] = B[--j];
        self->histogram[0]--;
        self->histogram[SPARSE_FSB(A[w])]++;
    }

    /* Close the gap left by duplicates */
    if (w > i) {
        memmove(A + i, A + w, (needed - w) * sizeof(SparseEntry));
    }

    self->sparseCount = needed - dupes;

    /* Check if we should convert to dense */
    uint64_t denseBytes = (self->size * 6) / 8 + 1;
    if (self->sparseCapacity * sizeof(SparseEntry) >= denseBytes) {
        if (transformToDense(self) < 0) {
            return -1;
        }
    }

    return 0;
}


/* Merges another HyperLogLog into the current HyperLogLog. The registers of
 * the other HyperLogLog are unaffected. */
static PyObject* HyperLogLog_merge(HyperLogLog* self, PyObject* args)
{
    HyperLogLog* otherHLL;
    uint64_t otherSize;

    if (!PyArg_ParseTuple(args, "O", &otherHLL)) return NULL;

    if (!PyObject_TypeCheck((PyObject*)otherHLL, Py_TYPE(self))) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a HyperLogLog instance");
        return NULL;
    }

    otherSize = otherHLL->size;

    if (otherSize != self->size) {
        PyErr_SetString(PyExc_ValueError, "Unequal sizes");
        return NULL;
    }

    self->isCached = 0;

    /* Fast path: both sparse — two-pointer merge O(countA + countB) */
    if (self->isSparse && otherHLL->isSparse) {
        if (mergeSparse(self, otherHLL) < 0) return NULL;
    } else {
        for (uint64_t i = 0; i < self->size; i++) {
            uint64_t newVal;
            uint64_t oldVal;

            if (self->isSparse) {
                oldVal = getSparseRegister(self, i);
            } else {
                oldVal = getDenseRegister(i, self->registers);
            }

            if (otherHLL->isSparse) {
                newVal = getSparseRegister(otherHLL, i);
            } else {
                newVal = getDenseRegister(i, otherHLL->registers);
            }

            if (oldVal < newVal) {
                if (setRegister(self, i, (uint8_t)newVal) < 0) return NULL;
            }
        }
    }

    Py_INCREF(Py_None);
    return Py_None;
}


/* Forward declarations for JMLE helper functions (defined below in helper section) */
static double mlEstimate(const uint64_t* c, unsigned p, unsigned q, double relerr);
static void buildJointHistogram(HyperLogLog* a, HyperLogLog* b,
    uint64_t* c1, uint64_t* c2, uint64_t* cu,
    uint64_t* ceq, uint64_t* cg1, uint64_t* cg2);

/* Estimate the intersection cardinality with another HyperLogLog using
 * Ertl's Joint Maximum Likelihood Estimation (JMLE) method. */
static PyObject* HyperLogLog_intersection_cardinality(HyperLogLog* self, PyObject* args)
{
    HyperLogLog* other;
    unsigned p, q;
    uint64_t m;
    double cAX, cBX, cABX, cAXBhalf, cBXAhalf;
    double cX1, cX2, cardX;
    uint64_t result;

    if (!PyArg_ParseTuple(args, "O", &other)) return NULL;

    if (!PyObject_TypeCheck((PyObject*)other, Py_TYPE(self))) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a HyperLogLog instance");
        return NULL;
    }

    if (other->size != self->size) {
        PyErr_SetString(PyExc_ValueError, "Unequal sizes");
        return NULL;
    }

    p = self->p;
    q = 64 - p;
    m = self->size;

    /* Flush sparse buffers before reading registers */
    if (self->isSparse && self->bufferSize > 0) {
        flushRegisterBuffer(self);
    }
    if (other->isSparse && other->bufferSize > 0) {
        flushRegisterBuffer(other);
    }

    /* Stack-allocate joint histogram arrays */
    uint64_t c1[66] = {0};  /* Sketch A histogram */
    uint64_t c2[66] = {0};  /* Sketch B histogram */
    uint64_t cu[66] = {0};  /* Union (max) histogram */
    uint64_t ceq[66] = {0}; /* Equal counts by value */
    uint64_t cg1[66] = {0}; /* Counts where A > B by A's value */
    uint64_t cg2[66] = {0}; /* Counts where B > A by B's value */

    buildJointHistogram(self, other, c1, c2, cu, ceq, cg1, cg2);

    /* Individual and union MLE estimates */
    cAX  = mlEstimate(c1, p, q, 1e-2);
    cBX  = mlEstimate(c2, p, q, 1e-2);
    cABX = mlEstimate(cu, p, q, 1e-2);

    /* Build half-range histograms for the JMLE refinement.
     * These combine joint histogram bins at shifted precision (q-1). */
    uint64_t countsAXBhalf[66] = {0};
    uint64_t countsBXAhalf[66] = {0};
    countsAXBhalf[q] = m;
    countsBXAhalf[q] = m;

    for (unsigned k = 0; k < q; k++) {
        countsAXBhalf[k] = cg1[k] + ceq[k] + cg2[k + 1];
        countsAXBhalf[q] -= countsAXBhalf[k];

        countsBXAhalf[k] = cg2[k] + ceq[k] + cg1[k + 1];
        countsBXAhalf[q] -= countsBXAhalf[k];
    }

    cAXBhalf = mlEstimate(countsAXBhalf, p, q - 1, 1e-2);
    cBXAhalf = mlEstimate(countsBXAhalf, p, q - 1, 1e-2);

    /* Combine two independent intersection estimators (Ertl JMLE formula) */
    cX1 = 1.5 * cBX + 1.5 * cAX - cBXAhalf - cAXBhalf;
    cX2 = 2.0 * (cBXAhalf + cAXBhalf) - 3.0 * cABX;
    cardX = 0.5 * (cX1 + cX2);

    result = (cardX > 0.5) ? (uint64_t)round(cardX) : 0;
    return Py_BuildValue("K", result);
}


static PyObject* HyperLogLog_new(PyTypeObject* type, PyObject*args, PyObject* kwds)
{
    HyperLogLog* self;
    self = (HyperLogLog*)type->tp_alloc(type, 0);
    return (PyObject*)self;
}


/*
 * Serialization method to pickle a HyperLogLog object.
 *
 * HyperLogLog's are serialized using a single Python list. The first 7
 * elements are fields, the next 65 elements are the histogram, and the
 * remaining elements represent the registers. Let N be the total number of
 * elements in the list, then the serialization schema is:
 *
 *     Index  Description
 *     -----  -----------
 *     0      isSparse field
 *     1      added field
 *     2      sparseCount field
 *     3      isCached field
 *     4      cache field
 *     5      (reserved)
 *     6      (reserved)
 *     7-71   register histogram values
 *     72-N   register values, if sparse then pairs of the form (register
 *            index, register value) otherwise integers
 */
static PyObject* HyperLogLog_reduce(HyperLogLog* self)
{
    PyObject* val;
    PyObject* state;
    uint64_t dumpSize;

    if (self->isSparse) {
        flushRegisterBuffer(self);
        dumpSize = self->sparseCount + 65 + 7;
    } else {
        dumpSize = self->size + 65 + 7;
    }

    state = PyList_New(dumpSize);

    for (uint64_t i = 0; i < dumpSize; i++) {
        val = Py_BuildValue("k", 0);
        PyList_SetItem(state, i, val);
    }

    PyList_SetItem(state, 0, Py_BuildValue("k", (uint64_t)self->isSparse));
    PyList_SetItem(state, 1, Py_BuildValue("k", self->added));
    PyList_SetItem(state, 2, Py_BuildValue("k", self->sparseCount));
    PyList_SetItem(state, 3, Py_BuildValue("k", self->isCached));
    PyList_SetItem(state, 4, Py_BuildValue("k", self->cache));
    PyList_SetItem(state, 5, Py_BuildValue("k", 0));
    PyList_SetItem(state, 6, Py_BuildValue("k", 0)); /* reserved */

    /* Set histogram values */
    for (int i = 7; i < 72; i++) {
        val = Py_BuildValue("k", self->histogram[i - 7]);
        PyList_SetItem(state, i, val);
    }

    if (self->isSparse) { /* Handle sparse representation */
        PyObject *pyList = NULL;

        for (uint64_t j = 0; j < self->sparseCount; j++) {
            pyList = PyList_New(2);
            PyList_SetItem(pyList, 0, Py_BuildValue("k", (unsigned long)SPARSE_INDEX(self->sparseRegisters[j])));
            PyList_SetItem(pyList, 1, Py_BuildValue("k", (unsigned long)SPARSE_FSB(self->sparseRegisters[j])));
            PyList_SetItem(state, 72 + j, pyList);
        }
    } else { /* Handle dense representation */
        for (uint64_t i = 72; i < self->size + 72; i++) {
            val = Py_BuildValue("k", getDenseRegister(i - 72, self->registers));
            PyList_SetItem(state, i, val);
        }
    }

    PyObject* args = Py_BuildValue("(iii)", self->p, self->seed, self->isSparse);
    return Py_BuildValue("(ONN)", Py_TYPE(self), args, state);
}


/* Gets the seed value used in the Murmur hash. */
static PyObject* HyperLogLog_seed(HyperLogLog* self)
{
    return Py_BuildValue("k", self->seed);
}


/* De-serialization method used to restore pickled objects. */
static PyObject* HyperLogLog_set_state(HyperLogLog* self, PyObject* state)
{

    PyObject* dump;
    PyObject* valPtr;
    unsigned long val;

    if (!PyArg_ParseTuple(state, "O:setstate", &dump)) return NULL;

    self->isSparse = (bool) PyLong_AsUnsignedLong(PyList_GetItem(dump, 0));
    self->added    = PyLong_AsUnsignedLong(PyList_GetItem(dump, 1));
    self->sparseCount = PyLong_AsUnsignedLong(PyList_GetItem(dump, 2));
    self->isCached = (bool) PyLong_AsUnsignedLong(PyList_GetItem(dump, 3));
    self->cache    = PyLong_AsUnsignedLong(PyList_GetItem(dump, 4));

    uint64_t dumpSize = self->isSparse ? self->sparseCount : self->size;
    dumpSize += 65 + 7;

    for (int i = 7; i < 65 + 7; i++) {
        valPtr = PyList_GetItem(dump, i);
        val = PyLong_AsUnsignedLong(valPtr);
        self->histogram[i - 7] = val;
    }

    if (self->isSparse) {
        uint64_t index;
        uint64_t fsb;
        PyObject *lst = NULL;

        /* Allocate exact capacity for the deserialized entries */
        self->sparseCapacity = self->sparseCount;
        self->sparseRegisters = (SparseEntry*)malloc(
            self->sparseCapacity * sizeof(SparseEntry));

        for (uint64_t i = 0; i < self->sparseCount; i++) {
            lst = PyList_GetItem(dump, 72 + i);
            index = PyLong_AsUnsignedLong(PyList_GetItem(lst, 0));
            fsb = PyLong_AsUnsignedLong(PyList_GetItem(lst, 1));

            self->sparseRegisters[i] = SPARSE_ENTRY(index, fsb);
        }
    } else {
        for (uint64_t i = 65 + 7; i < dumpSize; i++) {
            valPtr = PyList_GetItem(dump, i);
            val = PyLong_AsUnsignedLong(valPtr);
            setDenseRegister(i - 72, (uint8_t)val, self->registers);
        }
    }

    Py_INCREF(Py_None);
    return Py_None;
}


/* Gets the number of registers. */
static PyObject* HyperLogLog_size(HyperLogLog* self)
{
    return Py_BuildValue("i", self->size);
}


static PyMemberDef HyperLogLog_members[] = {
    {NULL} /* Sentinel */
};


static PyMethodDef HyperLogLog_methods[] = {
    {"add", (PyCFunction)HyperLogLog_add, METH_VARARGS,
     "Add an element."
    },
    {"add_range", (PyCFunction)HyperLogLog_add_range, METH_VARARGS,
     "Add a range of sequential integers [start, start+count) as 8-byte LE."
    },
    {"cardinality", (PyCFunction)HyperLogLog_cardinality, METH_NOARGS,
     "Get the cardinality."
    },
    {"merge", (PyCFunction)HyperLogLog_merge, METH_VARARGS,
     "Merge another HyperLogLog."
    },
    {"intersection_cardinality", (PyCFunction)HyperLogLog_intersection_cardinality, METH_VARARGS,
     "Estimate the intersection cardinality with another HyperLogLog using Ertl's JMLE method."
    },
    {"hash", (PyCFunction)HyperLogLog_hash, METH_VARARGS,
     "Get a MurmurHash64A hash."
    },
    {"seed", (PyCFunction)HyperLogLog_seed, METH_NOARGS,
     "Get the hash function seed."
    },
    {"size", (PyCFunction)HyperLogLog_size, METH_NOARGS,
     "Get the number of registers."
    },
    {"get_register", (PyCFunction)HyperLogLog_get_register, METH_VARARGS,
     "Get the value of a register."
    },
    {"registers", (PyCFunction)HyperLogLog_registers, METH_NOARGS,
     "Get all register values as a bytes object."
    },
    {"_histogram", (PyCFunction)HyperLogLog__histogram, METH_NOARGS,
     "Get a histogram of the register values."
    },
    {"_get_meta", (PyCFunction)HyperLogLog__get_meta, METH_NOARGS,
     "Get the values of internal attributes."
    },
    {"__reduce__", (PyCFunction)HyperLogLog_reduce, METH_NOARGS,
     "Serialization helper function for pickling."
    },
    {"__setstate__", (PyCFunction)HyperLogLog_set_state, METH_VARARGS,
    "De-serialization helper function for pickling."
    },
    {NULL}  /* Sentinel */
};


static PyTypeObject HyperLogLogType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "HLL.HyperLogLog",                        /* tp_name */
    sizeof(HyperLogLog),                      /* tp_basicsize */
    0,                                        /* tp_itemsize */
    (destructor)HyperLogLog_dealloc,          /* tp_dealloc */
    0,                                        /* tp_print */
    0,                                        /* tp_getattr */
    0,                                        /* tp_setattr */
    0,                                        /* tp_compare */
    0,                                        /* tp_repr */
    0,                                        /* tp_as_number */
    0,                                        /* tp_as_sequence */
    0,                                        /* tp_as_mapping */
    0,                                        /* tp_hash */
    0,                                        /* tp_call */
    0,                                        /* tp_str */
    0,                                        /* tp_getattro */
    0,                                        /* tp_setattro */
    0,                                        /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    "HyperLogLog object",                     /* tp_doc */
    0,                                        /* tp_traverse */
    0,                                        /* tp_clear */
    0,                                        /* tp_richcompare */
    0,                                        /* tp_weaklistoffset */
    0,                                        /* tp_iter */
    0,                                        /* tp_iternext */
    HyperLogLog_methods,                      /* tp_methods */
    HyperLogLog_members,                      /* tp_members */
    0,                                        /* tp_getset */
    0,                                        /* tp_base */
    0,                                        /* tp_dict */
    0,                                        /* tp_descr_get */
    0,                                        /* tp_descr_set */
    0,                                        /* tp_dictoffset */
    (initproc)HyperLogLog_init,               /* tp_init */
    0,                                        /* tp_alloc */
    HyperLogLog_new,                          /* tp_new */
};


static PyModuleDef HyperLogLogmodule = {
    PyModuleDef_HEAD_INIT,
    "HyperLogLog",
    "A space efficient cardinality estimator.",
    -1,
    NULL, NULL, NULL, NULL, NULL
};

PyMODINIT_FUNC
PyInit_HLL(void)
{
    PyObject* m;
    if (PyType_Ready(&HyperLogLogType) < 0) return NULL;
    m = PyModule_Create(&HyperLogLogmodule);
    if (m == NULL) return NULL;

    Py_INCREF(&HyperLogLogType);
    PyModule_AddObject(m, "HyperLogLog", (PyObject*)&HyperLogLogType);

    return m;
}


/* ========================== Helper functions ============================= */

/* Counts leading zeros (number of consecutive of zero bits from the left) in
 * an unsigned 64bit integer. */
static inline uint8_t clz(uint64_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return x ? __builtin_clzll(x) : 64;
#else
    static const uint8_t zeroes[] = {
        64, 63, 62, 62, 61, 61, 61, 61,
        60, 60, 60, 60, 60, 60, 60, 60,
        59, 59, 59, 59, 59, 59, 59, 59,
        59, 59, 59, 59, 59, 59, 59, 59,
        58, 58, 58, 58, 58, 58, 58, 58,
        58, 58, 58, 58, 58, 58, 58, 58,
        58, 58, 58, 58, 58, 58, 58, 58,
        58, 58, 58, 58, 58, 58, 58, 58,
        57, 57, 57, 57, 57, 57, 57, 57,
        57, 57, 57, 57, 57, 57, 57, 57,
        57, 57, 57, 57, 57, 57, 57, 57,
        57, 57, 57, 57, 57, 57, 57, 57,
        57, 57, 57, 57, 57, 57, 57, 57,
        57, 57, 57, 57, 57, 57, 57, 57,
        57, 57, 57, 57, 57, 57, 57, 57,
        57, 57, 57, 57, 57, 57, 57, 57,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56
    };

    uint8_t shift;

    if (x >= (1ULL << 32)) {
        if (x >= (1ULL << 48)) {
            shift = (x >= (1ULL << 56)) ? 56 : 48;
        } else {
            shift = (x >= (1ULL << 40)) ? 40 : 32;
        }
    } else {
        if (x >= (1U << 16)) {
            shift = (x >= (1U << 24)) ? 24 : 16;
        } else {
            shift = (x >= (1U << 8)) ? 8 : 0;
        }
    }

    uint8_t fsbByte = (uint8_t)(x >> shift);
    return zeroes[fsbByte] - shift;
#endif
}


static inline double sigma(double x) {
    if (x == 1.0) {
        return INFINITY;
    }

    double zPrime;
    double y = 1.0;
    double z = x;

    do {
        x *= x;
        zPrime = z;
        z += x*y;
        y += y;
    } while(z != zPrime);

    return z;
}


static inline double tau(double x) {
    if (x == 0.0 || x == 1.0) {
        return 0.0;
    }

    double zPrime;
    double y = 1.0;
    double z = 1 - x;

    do {
        x = sqrt(x);
        zPrime = z;
        y *= 0.5;
        z -= pow(1 - x, 2)*y;
    } while(zPrime != z);

    return z/3;
}


/* =================== Intersection cardinality (JMLE) ===================== */

/*
 * Maximum likelihood cardinality estimator for a single HyperLogLog sketch.
 * Uses the iterative secant method from Ertl (2017), Algorithm 5.
 *
 * Based on the reference implementation in dnbaker/sketch.
 *
 * c:      register histogram c[0..q+1]
 * p:      precision parameter
 * q:      64 - p (number of non-index bits)
 * relerr: relative error tolerance for convergence (default 1e-2)
 *
 * Returns the MLE cardinality estimate.
 */
static double mlEstimate(const uint64_t* c, unsigned p, unsigned q, double relerr)
{
    uint64_t m = 1ULL << p;
    int kMin, kMax;
    int kMinPrime, kMaxPrime;
    double z, gprev, x, a, deltaX;
    unsigned cPrime;
    int mPrime;

    if (c[q + 1] == m) return INFINITY;

    /* Find range of non-zero histogram bins */
    for (kMin = 0; c[kMin] == 0; kMin++) {}
    kMinPrime = kMin > 1 ? kMin : 1;
    for (kMax = (int)q + 1; kMax > 0 && c[kMax] == 0; kMax--) {}
    kMaxPrime = kMax < (int)q ? kMax : (int)q;

    /* Initial estimate z from normalized harmonic sum */
    z = 0.0;
    for (int k = kMaxPrime; k >= kMinPrime; k--) {
        z = 0.5 * z + (double)c[k];
    }
    z = ldexp(z, -kMinPrime);

    cPrime = (unsigned)c[q + 1];
    if (q > 0) cPrime += (unsigned)c[kMaxPrime];

    a = z + (double)c[0];
    mPrime = (int)m - (int)c[0];
    gprev = z + ldexp((double)c[q + 1], -(int)q);

    if (gprev <= 1.5 * a) {
        x = (double)mPrime / (0.5 * gprev + a);
    } else {
        x = ((double)mPrime / gprev) * log1p(gprev / a);
    }

    gprev = 0.0;
    deltaX = x;
    relerr /= sqrt((double)m);

    while (deltaX > x * relerr) {
        int kappaMinus1;
        frexp(x, &kappaMinus1);

        int shift = kMaxPrime + 1;
        if (kappaMinus1 + 2 > shift) shift = kappaMinus1 + 2;
        double xPrime = ldexp(x, -shift);
        double xPrime2 = xPrime * xPrime;
        double h = xPrime - xPrime2 / 3.0
                   + (xPrime2 * xPrime2) * (1.0/45.0 - xPrime2 / 472.5);

        for (int k = kappaMinus1; k >= kMaxPrime; k--) {
            double hPrime = 1.0 - h;
            h = (xPrime + h * hPrime) / (xPrime + hPrime);
            xPrime += xPrime;
        }

        double g = (double)cPrime * h;
        for (int k = kMaxPrime - 1; k >= kMinPrime; k--) {
            double hPrime = 1.0 - h;
            h = (xPrime + h * hPrime) / (xPrime + hPrime);
            xPrime += xPrime;
            g += (double)c[k] * h;
        }
        g += x * a;

        if (gprev < g && g <= (double)mPrime) {
            deltaX *= (g - (double)mPrime) / (gprev - g);
        } else {
            deltaX = 0;
        }

        x += deltaX;
        gprev = g;
    }

    return x * (double)m;
}


/*
 * Build joint register histograms for two HyperLogLog sketches.
 * Classifies each register position by comparing values from both sketches.
 *
 * Both HLLs must have buffers flushed before calling.
 *
 * c1[k]:  count of positions where register_a[i] == k (sketch A histogram)
 * c2[k]:  count of positions where register_b[i] == k (sketch B histogram)
 * cu[k]:  count of positions where max(register_a[i], register_b[i]) == k (union)
 * ceq[k]: count of positions where register_a[i] == register_b[i] == k
 * cg1[k]: count of positions where register_a[i] == k > register_b[i]
 * cg2[k]: count of positions where register_b[i] == k > register_a[i]
 */
static void buildJointHistogram(
    HyperLogLog* a, HyperLogLog* b,
    uint64_t* c1, uint64_t* c2, uint64_t* cu,
    uint64_t* ceq, uint64_t* cg1, uint64_t* cg2)
{
    uint64_t m = a->size;

    for (uint64_t i = 0; i < m; i++) {
        uint64_t va, vb;

        if (a->isSparse) {
            va = getSparseRegister(a, i);
        } else {
            va = getDenseRegister(i, a->registers);
        }

        if (b->isSparse) {
            vb = getSparseRegister(b, i);
        } else {
            vb = getDenseRegister(i, b->registers);
        }

        c1[va]++;
        c2[vb]++;

        if (va == vb) {
            cu[va]++;
            ceq[va]++;
        } else if (va > vb) {
            cu[va]++;
            cg1[va]++;
        } else {
            cu[vb]++;
            cg2[vb]++;
        }
    }
}


/* Print the bits in a byte. */
void printByte(uint8_t b)
{
    for (int i = 0; i < 8; i++) {
        printf("%d", !!((b << i) & 0x80));
    }
}


/* Check if a register index is valid, if not then set an error message. */
uint8_t isValidIndex(uint64_t index, uint64_t size)
{
    uint8_t valid = 1;

    if (index > size - 1) {
        char* msg = "Index exceeds the number of registers.";
        PyErr_SetString(PyExc_IndexError, msg);
        valid = 0;
    }

    return valid;
}
