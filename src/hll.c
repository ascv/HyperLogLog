#define PY_SSIZE_T_CLEAN
#define HLL_VERSION "2.0.1"

#include <math.h>
#include <Python.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "hll.h"
#include "structmember.h"
#include "../lib/murmur2.h"

typedef struct {
    PyObject_HEAD
    char* registers; /* Densely encoded registers */
    unsigned short p; /* 2^p = number of registers */
    uint64_t * histogram; /* Register histogram */
    uint64_t seed; /* MurmurHash64A seed */
    uint64_t size; /* Number of registers */
    uint64_t cache; /* Cached cardinality estimate */
    uint64_t added; /* Number of elements added */
    bool isCached; /* If the cache is up to date */
    bool isSparse; /* If sparse encoding is currently in use */

    /* Fields used for sparse representation */
    struct Node* sparseRegisterList; /* Linked list of registers */
    struct Node* sparseRegisterBuffer; /* Temporary buffer of nodes to be added */
    struct Node* nodeCache; /* The last sparse register accessed using _get() */
    uint64_t bufferSize; /* Number of elements in the temporary buffer */
    uint64_t listSize; /* Number of elements in the linked list */
    uint64_t maxBufferSize; /* Max number of elements for the temporary buffer */
    uint64_t maxListSize; /* Max number of nodes in the sparse list */
} HyperLogLog;

typedef struct Node {
    struct Node* next;
    uint64_t index;
    char fsb;
} Node;


/* ========================== Dense representation ========================= */
/*
 * Since register values will never exceed 64 we store them using only 6 bits.
 * This encoding is diagrammed below:
 *
 *          b0        b1        b3        b4
 *          /         /         /         /
 *     +-------------------+---------+---------+
 *     |0000 0011|1111 0011|0110 1110|1111 1011|
 *     +-------------------+---------+---------+
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
 * Since the bytes have been updated, we're done. The final result is shown
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
static inline uint64_t getDenseRegister(uint64_t m, char* regs)
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
static inline void setDenseRegister(uint64_t m, uint8_t n, char* regs)
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
 * them individually. Instead only non-zero registers are stored. These
 * registers are stored as nodes in a sorted link list. For example the
 * registers
 *
 *     +-+-+-+-+-+-+-+-+
 *     |0|3|0|0|1|1|0|2|
 *     +-+-+-+-+-+-+-+-+
 *
 * are represented with the following linked list (sorted by index):
 *
 *            index
 *              |
 *              v
 *     +---+   +---+   +---+   +---+
 *     |1,3|-->|4,1|-->|5,1|-->|7,2|
 *     +---+   +---+   +---+   +---+
 *                ^
 *                |
 *              value
 *
 * To avoid the worst case of traversing the linked list every time add()
 * is called a temporary register buffer is used to store the new register
 * values. When the buffer is full it is sorted and the linked list is
 * updated. Because both the list and buffer are sorted this update can be
 * done in one pass.
 *
 * Eventually the linked list grows too large to save memory. When this
 * happens the HyperLogLog switches to a dense representation.
 */


/* Compares two sparse register nodes. */
int compareNodes(const void* a, const void* b) {

    int result = -1;
    struct Node* A = (struct Node*) a;
    struct Node* B = (struct Node*) b;

    if (A->index == B->index) {
        if (A->fsb > B->fsb) {
            result = 1;
        }
        else if (A->fsb < B->fsb) {
            result = -1;
        }
        else {
            result = 0;
        }
    }

    else if (A->index > B->index) {
        result = 1;
    }

    return result;
}


/* Updates the register list using the items in the buffer. */
void flushRegisterBuffer(HyperLogLog* self)
{
    uint64_t i;
    struct Node* node;
    struct Node *current = self->sparseRegisterList;
    struct Node *next = NULL;
    struct Node *prev = NULL;

    qsort(self->sparseRegisterBuffer, self->bufferSize, sizeof(struct Node), compareNodes);

    for (i = 0; i < self->bufferSize; i++) {

        /* Create the new node from the current item in the buffer */
        node = (struct Node*)malloc(sizeof(struct Node));
        node->fsb = self->sparseRegisterBuffer[i].fsb;
        node->index = self->sparseRegisterBuffer[i].index;
        node->next = NULL;

        /* If head doesn't exist then set it */
        if (self->sparseRegisterList == NULL) {
            self->sparseRegisterList = node;
            self->histogram[0]--;
            self->histogram[(uint8_t)node->fsb]++;
            self->listSize += 1;
            prev = node;
            continue;
        }

        /* Since both lists are sorted try to use the last node */
        if (prev != NULL) {
            current = prev;
        }
        else {
            current = self->sparseRegisterList;
        }

        while (current != NULL) {

            // Are we updating an existing node?
            if (current->index == node->index) {
                if (current->fsb < node->fsb) {
                    self->histogram[(uint8_t)current->fsb]--;
                    self->histogram[(uint8_t)node->fsb]++;
                    current->fsb = node->fsb;
                }
                prev = current;

                /* We don't need the new node */
                free(node);

                break;
            }

            // Are we creating a new head?
            if (current->index > node->index) {
                node->next = current;
                self->histogram[0]--;
                self->histogram[(uint8_t)node->fsb]++;
                self->sparseRegisterList = node;
                self->listSize += 1;
                prev = node;
                break;
            }

            // Are we creating a new tail?
            if (current->next == NULL) {
                current->next = node;
                self->histogram[0]--;
                self->histogram[(uint8_t)node->fsb]++;
                self->listSize += 1;
                prev = node;
                break;
            }

            // Are inserting between nodes?
            if (current->next->index > node->index) {
                node->next = current->next;
                current->next = node;
                self->histogram[0]--;
                self->histogram[(uint8_t)node->fsb]++;
                self->listSize += 1;
                prev = node;
                break;
            }

            next = current->next;
            current = next;
        }
    }

    self->bufferSize = 0;
}


/* Gets the register value at the specified index. */
static inline uint64_t getSparseRegister(HyperLogLog* self, uint64_t index)
{
    struct Node *current = NULL;

    if (self->bufferSize > 0) {
        flushRegisterBuffer(self);
    }

    current = self->sparseRegisterList;

    /* Can we used the cache? */
    if (self->nodeCache != NULL && self->nodeCache->index <= index) {
        current = self->nodeCache;
    }

    while (current != NULL) {

        if (current->index > index) {
            return 0;
        }

        else if (current->index == index) {
            self->nodeCache = current;
            return current->fsb;
        }

        current = current->next;
    }

    return 0;
}


/* ====================== HyperLogLog object methods ======================= */


/* Gets the a register value by index */
static PyObject*
HyperLogLog__get_register(HyperLogLog* self, PyObject* args)
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


/* Gets a dictionary of internal attributes and their values */
static PyObject*
HyperLogLog__get_meta(HyperLogLog* self, PyObject* args)
{
    char version[8];
    sprintf(version, "%u.%u.%u", PY_MAJOR_VERSION, PY_MINOR_VERSION, PY_MICRO_VERSION);

    uint64_t cacheIndex = self->nodeCache == NULL ? 0 : self->nodeCache->index;
    uint64_t cacheValue = self->nodeCache == NULL ? 0 : self->nodeCache->fsb;

    return Py_BuildValue("{s:k,s:k,s:k,s:k,s:i,s:i,s:k,s:k,s:s,s:s}",
        "added", self->added,
        "list_size", self->listSize,
        "buffer_size", self->bufferSize,
        "cache", self->cache,
        "is_cached", self->isCached,
        "is_sparse", self->isSparse,
        "node_cache_index", cacheIndex,
        "node_cache_value", cacheValue,
        "py_version", version,
        "hll_version", HLL_VERSION
    );
}


/* Gets a histogram of first set bit positions as a list of ints. */
static PyObject*
HyperLogLog__histogram(HyperLogLog* self)
{
    PyObject* histogram = PyList_New(65);

    for (int i = 0; i < 65; i++)
    {
        PyObject* count = Py_BuildValue("i", self->histogram[i]);
        PyList_SetItem(histogram, i, count);
    }
    return histogram;
}


static void
HyperLogLog_dealloc(HyperLogLog* self)
{
    free(self->histogram);
    free(self->registers);

    if (self->isSparse) {
        struct Node *next = NULL;
        struct Node *current = self->sparseRegisterList;
        while (current != NULL) {
            next = current->next;
            free(current);
            current = next;
        }

        free(self->sparseRegisterBuffer);
    }

    Py_TYPE(self)->tp_free((PyObject*) self);
}


/* Add an element. */
static PyObject*
HyperLogLog_add(HyperLogLog* self, PyObject* args)
{
    const char* data;
    const uint64_t dataLen;
    uint64_t hash, index, fsb, newFsb;

    if (!PyArg_ParseTuple(args, "s#", &data, &dataLen)) return NULL;
    hash = MurmurHash64A((void*)data, dataLen, self->seed);

    index = (hash >> (64 - self->p)); /* Use the first p bits as an index */
    newFsb = hash << self->p; /* Remove the first p bits */
    newFsb = clz(newFsb) + 1; /* Find the first set bit in the remaining bits */
    self->added++; /* Increment method call counter */

    if (self->isSparse) {

        /* Add an element to the buffer if there is room */
        if (self->bufferSize < self->maxBufferSize) {
            self->sparseRegisterBuffer[self->bufferSize].index = index;
            self->sparseRegisterBuffer[self->bufferSize].fsb = newFsb;
            self->bufferSize++;
        }

        /* Otherwise flush the buffer and then add */
        else {
            flushRegisterBuffer(self);
            self->bufferSize = 1;
            self->sparseRegisterBuffer[0].index = index;
            self->sparseRegisterBuffer[0].fsb = newFsb;
        }

        /* Switch to dense representation? */
        if (self->listSize >= self->maxListSize) {

            uint64_t bytes = (self->size*6)/8 + 1;
            self->registers = (char *)calloc(bytes, sizeof(char));

            if (self->registers == NULL) {
                char* msg = (char*) malloc(128 * sizeof(char));
                sprintf(msg, "Failed to allocate %lu bytes.", bytes);
                PyErr_SetString(PyExc_MemoryError, msg);
                Py_RETURN_FALSE;
            }

            flushRegisterBuffer(self);

            struct Node *next = NULL;
            struct Node *current = self->sparseRegisterList;

            while (current != NULL) {
                setDenseRegister(current->index, (uint8_t)current->fsb, self->registers);
                next = current->next;
                current = next;
            }

            current = self->sparseRegisterList;

            while (current != NULL) {
                next = current;
                current = current->next;
                free(next);
            }

            free(self->sparseRegisterBuffer);
            free(self->nodeCache);
            self->isSparse = 0;
        }

        self->isCached = 0;
    }

    else {
        fsb = getDenseRegister(index, self->registers);

        if (newFsb > fsb) {
            setDenseRegister(index, (uint8_t)newFsb, self->registers);
            self->histogram[newFsb] += 1; /* Increment the new count */
            self->isCached = 0;

            if (self->histogram[fsb] == 0) {
                self->histogram[0] -= 1;
            }

            else {
                self->histogram[fsb] -= 1;
            }

            Py_RETURN_TRUE;
        }
    }

    Py_RETURN_FALSE;
};


/* Get a cardinality estimate */
static PyObject*
HyperLogLog_cardinality(HyperLogLog* self)
{
    if (self->isCached) {
        return Py_BuildValue("K", self->cache);
    }

    else if (self->isSparse && self->bufferSize > 0) {
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
    uint64_t estimate = (uint64_t) round(alpha*m*(m/z));

    self->cache = estimate;
    self->isCached = 1;

    return Py_BuildValue("K", estimate);
}


/* Get a Murmur64A hash of a string, buffer or bytes object. */
static PyObject*
HyperLogLog_hash(HyperLogLog* self, PyObject* args)
{
    const char* data;
    const uint64_t dataLen;

    if (!PyArg_ParseTuple(args, "s#", &data, &dataLen)) return NULL;

    uint64_t hash = MurmurHash64A((void*) data, dataLen, self->seed);
    return Py_BuildValue("K", hash);
}


static int
HyperLogLog_init(HyperLogLog* self, PyObject* args, PyObject* kwds)
{
    static char* kwlist[] = {"p", "seed", "sparse", "max_sparse_list_size", "max_sparse_buffer_size", NULL};
    uint64_t maxSparseListSize = 0;
    uint64_t maxSparseBufferSize = 0;
    int64_t sparse = 0;

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
    self->listSize = 0;
    self->size = 1UL << self->p;
    self->histogram = (uint64_t*)calloc(65, sizeof(uint64_t)); /* Keep a count of register values */
    self->histogram[0] = self->size; /* Set the zeroes count */

    if (sparse) {
        self->isSparse = 1;

        if (maxSparseListSize > 0) {
            self->maxListSize = maxSparseListSize;
        }
        else {
            uint64_t defaultSize = self->size/4;
            uint64_t maxDefaultSize = 1 << 20;

            if (maxDefaultSize < defaultSize) {
                self->maxListSize = maxDefaultSize;
            }
            else if (defaultSize <= 4) { /* This shouldn't happen, but do something reasonable if it does */
                self->maxListSize = 2;
            }
            else {
                self->maxListSize = defaultSize;
            }
        }

        if (maxSparseBufferSize > 0) {
            self->maxBufferSize = maxSparseBufferSize;
        }
        else {
            uint64_t defaultSize = self->maxListSize/2;
            uint64_t maxDefaultSize = 200000;

            if (maxDefaultSize < defaultSize) {
                self->maxBufferSize = maxDefaultSize;
            }
            else {
                self->maxBufferSize = defaultSize;
            }
        }

        self->sparseRegisterBuffer = (struct Node*)malloc(sizeof(struct Node)* self->maxBufferSize);
    }
    else {
        uint64_t bytes = (self->size*6)/8 + 1;
        self->registers = (char *)calloc(bytes, sizeof(char));

        if (self->registers == NULL) {
            char* msg = (char*) malloc(128 * sizeof(char));
            sprintf(msg, "Failed to allocate %lu bytes. Use a smaller p.", bytes);
            PyErr_SetString(PyExc_MemoryError, msg);
            return -1;
        }
    }

    return 0;
}


/* Merges another HyperLogLog into the current HyperLogLog. The registers of
 * the other HyperLogLog are unaffected. */
static PyObject*
HyperLogLog_merge(HyperLogLog* self, PyObject* args)
{
    PyObject* hll;
    uint64_t hllSize;

    if (!PyArg_ParseTuple(args, "O", &hll)) return NULL;

    PyObject* size = PyObject_CallMethod(hll, "size", NULL);

    #if PY_MAJOR_VERSION >= 3
        hllSize = PyLong_AsLong(size);
    #else
        hllSize = PyInt_AS_LONG(size);
    #endif

    if (hllSize > self->size) {
        PyErr_SetString(PyExc_ValueError, "Unequal sizes");
        return NULL;
    }

    Py_DECREF(size);
    self->isCached = 0;

    for (uint64_t i = 0; i < self->size; i++) {
        PyObject* newReg = PyObject_CallMethod(hll, "_get_register", "i", i);
        unsigned long newVal = PyLong_AsUnsignedLong(newReg);
        uint64_t oldVal = getDenseRegister(i, self->registers);

        if (oldVal < newVal) {
            setDenseRegister(i, newVal, self->registers);
            self->histogram[newVal] += 1; /* Increment new count */

            if (self->histogram[oldVal] > 0) {
                self->histogram[oldVal] -= 1; /* Decrement old count */
            }

            else {
                self->histogram[0] += 1; /* Increment zeroes count */
            }
        }

        Py_DECREF(newReg);
    }

    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject*
HyperLogLog_new(PyTypeObject* type, PyObject*args, PyObject* kwds)
{
    HyperLogLog* self;
    self = (HyperLogLog*)type->tp_alloc(type, 0);
    return (PyObject*)self;
}


/*
 * Serialization method to pickle a HyperLogLog object.
 *
 * HyperLogLog's are serialized using a single list. The first 7 elements
 * are fields, the next 65 elements are the histogram, and the remaining
 * elements represent the registers. Let N be the total number of elements in
 * in the list, then the serialization schema is:
 *
 *     Index  Description
 *     -----  -----------
 *     0      version
 *     1      isSparse field
 *     2      added field
 *     3      listSize field
 *     4      isCached field
 *     5      cache field
 *     6      index of node that is currently cached
 *     7-72   register histogram values
 *     73-N   register values, if sparse then tuples of the form (register
 *            index, register value) otherwise integers
 */
static PyObject*
HyperLogLog_reduce(HyperLogLog* self)
{
    PyObject* val;
    PyObject* state;
    uint64_t dumpSize;

    if (self->isSparse) {
        flushRegisterBuffer(self);
        dumpSize = self->listSize + 65 + 7;
    }

    else {
        dumpSize = self->size + 65 + 7;
    }

    state = PyList_New(dumpSize);

    for (uint64_t i = 0; i < dumpSize; i++) {
        val = Py_BuildValue("k", 0);
        PyList_SetItem(state, i, val);
    }

    PyList_SetItem(state, 0, Py_BuildValue("k", (uint64_t)self->isSparse));
    PyList_SetItem(state, 1, Py_BuildValue("k", self->added));
    PyList_SetItem(state, 2, Py_BuildValue("k", self->listSize));
    PyList_SetItem(state, 3, Py_BuildValue("k", self->isCached));
    PyList_SetItem(state, 4, Py_BuildValue("k", self->cache));
    PyList_SetItem(state, 5, Py_BuildValue("k", 0));
    PyList_SetItem(state, 6, Py_BuildValue("k", 0)); /* reserved */

    /* Set histogram values */
    for (int i = 7; i < 72; i++) {
        val = Py_BuildValue("k", self->histogram[i - 7]);
        PyList_SetItem(state, i, val);
    }

    /* Serialize sparse representation */
    if (self->isSparse) {

        if (self->nodeCache != NULL) {
            PyList_SetItem(state, 5, Py_BuildValue("k", self->nodeCache->index));
        }


        struct Node *current = NULL;
        PyObject *pyList = NULL;

        current = self->sparseRegisterList;
        uint64_t j = 72;

        while (current != NULL) {
            pyList = PyList_New(2);
            PyList_SetItem(pyList, 0, Py_BuildValue("k", current->index));
            PyList_SetItem(pyList, 1, Py_BuildValue("k", current->fsb));
            PyList_SetItem(state, j, pyList);
            current = current->next;
            j++;
        }
    }

    /* Serialize dense representation */
    else {
        for (uint64_t i = 72; i < self->size + 72; i++) {
            val = Py_BuildValue("k", getDenseRegister(i - 72, self->registers));
            PyList_SetItem(state, i, val);
        }
    }

    PyObject* args = Py_BuildValue("(iii)", self->p, self->seed, self->isSparse);
    return Py_BuildValue("(ONN)", Py_TYPE(self), args, state);
}


/* Gets the seed value used in the Murmur hash. */
static PyObject*
HyperLogLog_seed(HyperLogLog* self)
{
    return Py_BuildValue("k", self->seed);
}


/* De-serialization method used to restore pickled objects. */
static PyObject*
HyperLogLog_set_state(HyperLogLog* self, PyObject* state)
{

    PyObject* dump;
    PyObject* valPtr;
    unsigned long val;
    uint64_t nodeCacheIndex;

    if (!PyArg_ParseTuple(state, "O:setstate", &dump)) return NULL;

    self->isSparse = (bool) PyLong_AsUnsignedLong(PyList_GetItem(dump, 0));
    self->added    = PyLong_AsUnsignedLong(PyList_GetItem(dump, 1));
    self->listSize = PyLong_AsUnsignedLong(PyList_GetItem(dump, 2));
    self->isCached = (bool) PyLong_AsUnsignedLong(PyList_GetItem(dump, 3));
    self->cache    = PyLong_AsUnsignedLong(PyList_GetItem(dump, 4));
    nodeCacheIndex = PyLong_AsUnsignedLong(PyList_GetItem(dump, 5));

    uint64_t dumpSize = self->isSparse ? self->listSize : self->size;
    dumpSize += 65 + 7;

    for (int i = 7; i < 65 + 7; i++) {
        valPtr = PyList_GetItem(dump, i);
        val = PyLong_AsUnsignedLong(valPtr);
        self->histogram[i - 7] = val;
    }

    if (self->isSparse) {
        uint64_t index;
        uint64_t fsb;
        struct Node* node = NULL;
        struct Node* prev = NULL;
        PyObject *lst = NULL;

        self->sparseRegisterList = node;

        for (uint64_t i = 65 + 7; i < dumpSize; i++) {
            lst = PyList_GetItem(dump, i);
            index = PyLong_AsUnsignedLong(PyList_GetItem(lst, 0));
            fsb = PyLong_AsUnsignedLong(PyList_GetItem(lst, 1));

            node = (struct Node*)malloc(sizeof(struct Node));
            node->index = index;
            node->fsb = fsb;
            node->next = NULL;

            if (i == 65 + 7) {
                self->sparseRegisterList = node;
                prev = node;
            }
            else {
                prev->next = node;
            }

            if (node->index == nodeCacheIndex) {
                self->nodeCache = node;
            }
        }
    }
    else {
        for (uint64_t i = 65 + 7; i < dumpSize; i++) {
            valPtr = PyList_GetItem(dump, i);
            val = PyLong_AsUnsignedLong(valPtr);
            setDenseRegister(i-63, (uint8_t)val, self->registers);
        }
    }

    Py_INCREF(Py_None);
    return Py_None;
}


/* Gets the number of registers. */
static PyObject*
HyperLogLog_size(HyperLogLog* self)
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
    {"cardinality", (PyCFunction)HyperLogLog_cardinality, METH_NOARGS,
     "Get the cardinality."
    },
    {"merge", (PyCFunction)HyperLogLog_merge, METH_VARARGS,
     "Merge another HyperLogLog."
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
    {"_get_register", (PyCFunction)HyperLogLog__get_register, METH_VARARGS,
     "Get the value of a register."
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
    "HLL.HyperLogLog",               /* tp_name */
    sizeof(HyperLogLog),             /* tp_basicsize */
    0,                               /* tp_itemsize */
    (destructor)HyperLogLog_dealloc, /* tp_dealloc */
    0,                               /* tp_print */
    0,                               /* tp_getattr */
    0,                               /* tp_setattr */
    0,                               /* tp_compare */
    0,                               /* tp_repr */
    0,                               /* tp_as_number */
    0,                               /* tp_as_sequence */
    0,                               /* tp_as_mapping */
    0,                               /* tp_hash */
    0,                               /* tp_call */
    0,                               /* tp_str */
    0,                               /* tp_getattro */
    0,                               /* tp_setattro */
    0,                               /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT |
        Py_TPFLAGS_BASETYPE,         /* tp_flags */
    "HyperLogLog object",            /* tp_doc */
    0,                               /* tp_traverse */
    0,                               /* tp_clear */
    0,                               /* tp_richcompare */
    0,                               /* tp_weaklistoffset */
    0,                               /* tp_iter */
    0,                               /* tp_iternext */
    HyperLogLog_methods,             /* tp_methods */
    HyperLogLog_members,             /* tp_members */
    0,                               /* tp_getset */
    0,                               /* tp_base */
    0,                               /* tp_dict */
    0,                               /* tp_descr_get */
    0,                               /* tp_descr_set */
    0,                               /* tp_dictoffset */
    (initproc)HyperLogLog_init,      /* tp_init */
    0,                               /* tp_alloc */
    HyperLogLog_new,                 /* tp_new */
};


#if PY_MAJOR_VERSION >= 3
    static PyModuleDef HyperLogLogmodule = {
        PyModuleDef_HEAD_INIT,
        "HyperLogLog",
        "A space efficient cardinality estimator.",
        -1,
        NULL, NULL, NULL, NULL, NULL
    };
#else
    static PyMethodDef module_methods[] = {
        {NULL}  /* Sentinel */
    };
#endif

#if PY_MAJOR_VERSION >=3
    PyMODINIT_FUNC
    PyInit_HLL(void)
#else
    /* declarations for DLL import or export */
    #ifndef PyMODINIT_FUNC
        #define PyMODINIT_FUNC void
    #endif
    PyMODINIT_FUNC initHLL(void)
#endif

{
    PyObject* m;
    #if PY_MAJOR_VERSION >= 3
        if (PyType_Ready(&HyperLogLogType) < 0) return NULL;
        m = PyModule_Create(&HyperLogLogmodule);
        if (m == NULL) return NULL;
    #else
        if (PyType_Ready(&HyperLogLogType) < 0) return;
        char* info = "HyperLogLog cardinality estimator.";
        m = Py_InitModule3("HLL", module_methods, info);
        if (m == NULL) return;
    #endif

    Py_INCREF(&HyperLogLogType);
    PyModule_AddObject(m, "HyperLogLog", (PyObject*)&HyperLogLogType);

    #if PY_MAJOR_VERSION >= 3
        return m;
    #endif
}


/* ========================== Helper functions ============================= */

/* Counts leading zeros (number of consecutive of zero bits from the left) in
 * an unsigned 64bit integer. */
static inline uint8_t clz(uint64_t x) {

    uint8_t shift;

    static uint8_t const zeroes[] = {
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
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56
    };

    /* Do a binary search to find which byte contains the first set bit. */
    if (x >= (1UL << 32UL)) {
        if (x >= (1UL << 48UL)) {
            shift = (x >= (1UL << 56UL)) ? 56 : 48;
        } else {
            shift = (x >= (1UL << 38UL)) ? 40 : 32;
        }
    } else {
        if (x >= (1U << 16U)) {
            shift = (x >= (1U << 24U)) ? 24 : 16;
        } else {
            shift = (x >= (1U << 8U)) ? 8 : 0;
        }
    }

    /* Get the byte containing the first set bit. */
    uint8_t fsbByte = (uint8_t)(x >> shift);

    /* Look up the leading zero count for (x >> shift) using byte. Subtract
     * the bit shift to get the leading zero count for x. */
    return zeroes[fsbByte] - shift;
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


/* Print a the bits in a byte. */
void printByte(char b)
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
