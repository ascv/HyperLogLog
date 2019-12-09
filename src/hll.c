#define PY_SSIZE_T_CLEAN

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
    char * registers;      /* Contains the first set bit positions */
    unsigned short p;      /* 2^p = number of registers */
    uint64_t * histogram;  /* Register histogram */
    uint64_t seed;         /* MurmurHash64A seed */
    uint64_t size;         /* Number of registers */
    double cache;          /* Cached cardinality cardinality estimate */
    bool isCached;         /* If the cache is up to date */
} HyperLogLog;

static void
HyperLogLog_dealloc(HyperLogLog* self)
{
    free(self->registers);
    #if PY_MAJOR_VERSION >= 3
        Py_TYPE(self)->tp_free((PyObject*) self);
    #else
        self->ob_type->tp_free((PyObject*) self);
    #endif
}

static PyObject *
HyperLogLog_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    HyperLogLog *self;
    self = (HyperLogLog *)type->tp_alloc(type, 0);
    return (PyObject *)self;
}

static int
HyperLogLog_init(HyperLogLog *self, PyObject *args, PyObject *kwds)
{
    self->seed = 314;

    static char *kwlist[] = {"p", "seed"};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "i|i", kwlist, &self->p, &self->seed)) {
        return -1;
    }

    if (self->p < 2 || self->p > 63) {
        char * msg = "p is out of range";
        PyErr_SetString(PyExc_ValueError, msg);
        return -1;
    }

    self->size = 1UL << self->p;
    uint64_t bytes = (self->size*6)/8 + 1;
    self->registers = (char *)calloc(bytes, sizeof(char));

    if (self->registers == NULL) {
        setMemoryErrorMsg(bytes);
        return -1;
    }

    self->histogram = (uint64_t *)calloc(65, sizeof(uint64_t));
    self->histogram[0] = self->size;
    self->cache = 0.0;
    self->isCached = 0;

    return 0;
}

static PyMemberDef HyperLogLog_members[] = {
    {NULL} /* Sentinel */
};

/* Add an element. */
static PyObject *
HyperLogLog_add(HyperLogLog *self, PyObject *args)
{
    const char *data;
    const uint64_t dataLen;

    if (!PyArg_ParseTuple(args, "s#", &data, &dataLen)) {
        return NULL;
    }

    uint64_t hash, index, fsb, newFsb;

    hash = MurmurHash64A((void *)data, dataLen, self->seed);

    index = (hash >> (64 - self->p)); /* Use the first p bits as an index */
    fsb = getReg(index, self->registers); /* Pick a register */
    newFsb = hash << self->p; /* Remove the first p bits */
    newFsb = clz(newFsb) + 1; /* Find the first set bit */

    if (newFsb > fsb) {
        setReg(index, (uint8_t)newFsb, self->registers);
        self->histogram[newFsb] += 1;
        self->isCached = 0;

        if (self->histogram[fsb] > 0) {
            self->histogram[fsb] -= 1;
        }

        self->histogram[0] += 1;
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
};


/* Get a cardinality estimate */
static PyObject *
HyperLogLog_cardinality(HyperLogLog *self)
{
    if (self->isCached) {
        return Py_BuildValue("d", self->cache);
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
    double estimate = alpha*m*(m/z);

    self->cache = estimate;
    self->isCached = 1;

    return Py_BuildValue("d", estimate);
}

/* Gets the seed value used in the Murmur hash. */
static PyObject *
HyperLogLog__get_register(HyperLogLog* self, PyObject * args)
{
    unsigned long index;

    if (!PyArg_ParseTuple(args, "k", &index)) return NULL;
    if (!isValidIndex(index, self->size)) return NULL;

    return Py_BuildValue("k", getReg((uint8_t)index, self->registers));
}


/* Get a Murmur2 hash of a python string, buffer or bytes (python 3.x) as an
 * unsigned integer. */
static PyObject *
HyperLogLog_hash(HyperLogLog *self, PyObject *args)
{
    const char *data;
    const uint64_t dataLen;

    if (!PyArg_ParseTuple(args, "s#", &data, &dataLen)) {
        return NULL;
    }

    uint64_t hash = MurmurHash64A((void *) data, dataLen, self->seed);
    return Py_BuildValue("K", hash);
}


/* Gets a histogram of first set bit positions as a list of ints. */
static PyObject *
HyperLogLog__histogram(HyperLogLog *self)
{
    PyObject* histogram = PyList_New(64);

    /* Skip first value since it contains the size + count */
    for (int i = 1; i < 65; i++)
    {
        PyObject* count = Py_BuildValue("i", self->histogram[i]);
        PyList_SetItem(histogram, (i-1), count);
    }
    return histogram;
}


/* Merges another HyperLogLog into the current HyperLogLog. The registers of
 * the other HyperLogLog are unaffected. */
static PyObject *
HyperLogLog_merge(HyperLogLog *self, PyObject * args)
{
    PyObject *hll;
    if (!PyArg_ParseTuple(args, "O", &hll)) {
        return NULL;
    }

    PyObject *size = PyObject_CallMethod(hll, "size", NULL);

    #if PY_MAJOR_VERSION >= 3
        long hllSize = PyLong_AsLong(size);
    #else
        long hllSize = PyInt_AS_LONG(size);
    #endif

    if (hllSize > self->size) {
        PyErr_SetString(PyExc_ValueError, "");
        return NULL;
    }

    Py_DECREF(size);
    self->isCached = 0;

    for (uint64_t i = 0; i < self->size; i++) {
        PyObject *newReg = PyObject_CallMethod(hll, "_get_register", "i", i);
        unsigned long newVal = PyLong_AsUnsignedLong(newReg);
        uint64_t oldVal = getReg(i, self->registers);

        if (oldVal < newVal) {
            setReg(i, newVal, self->registers);
            self->histogram[newVal] += 1;
            self->histogram[oldVal] -= 1;
        }

        Py_DECREF(newReg);
    }

    Py_INCREF(Py_None);
    return Py_None;
}


/* Support for pickling, called when HyperLogLog is serialized. */
static PyObject *
HyperLogLog_reduce(HyperLogLog *self)
{
    char *arr = (char *) malloc(self->size * sizeof(char));

    /* Pickle protocol 2, used in python 2.x, doesn't allow null bytes in
     * strings and does not support pickling bytearrays. For backwards
     * compatibility, we set all null bytes to 'z' before pickling.
     */
    int i;
    for (i = 0; i < self->size; i++) {
        if (self->registers[i] == 0) {
            arr[i] = 'z';
        }

        else {
            arr[i] = self->registers[i];
        }
    }

    PyObject *args = Py_BuildValue("(ii)", self->p, self->seed);
    PyObject *registers = Py_BuildValue("s#", arr, self->size);

    free(arr);

    return Py_BuildValue("(ONN)", Py_TYPE(self), args, registers);
}


/* Gets the seed value used in the Murmur hash. */
static PyObject *
HyperLogLog_seed(HyperLogLog* self)
{
    return Py_BuildValue("k", self->seed);
}


/* Support for pickling, called when HyperLogLog is de-serialized. */
static PyObject *
HyperLogLog_set_state(HyperLogLog * self, PyObject * state)
{

    char *registers;
    if (!PyArg_ParseTuple(state, "s:setstate", &registers)) {
        return NULL;
    }

    int i;
    for (i = 0; i < self->size; i++) {
        if (registers[i] == 'z') {
            self->registers[i] = 0;
        } else {
            self->registers[i] = registers[i];
        }
    }

    Py_INCREF(Py_None);
    return Py_None;
}

/* Gets the number of registers. */
static PyObject *
HyperLogLog_size(HyperLogLog* self)
{
    return Py_BuildValue("i", self->size);
}

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
    {"_histogram", (PyCFunction)HyperLogLog__histogram, METH_NOARGS,
     "Get a histogram of the register values."
    },
    {"__reduce__", (PyCFunction)HyperLogLog_reduce, METH_NOARGS,
     "Serialization helper function for pickling."
    },
    {"_get_register", (PyCFunction)HyperLogLog__get_register, METH_VARARGS,
     "Get the value of a register."
    },
    {"seed", (PyCFunction)HyperLogLog_seed, METH_NOARGS,
     "Get the hash function seed."
    },
    {"__setstate__", (PyCFunction)HyperLogLog_set_state, METH_VARARGS,
    "De-serialization helper function for pickling."
    },
    {"size", (PyCFunction)HyperLogLog_size, METH_NOARGS,
     "Get the number of registers."
    },
    {NULL}  /* Sentinel */
};

static PyTypeObject HyperLogLogType = {
    #if PY_MAJOR_VERSION >= 3
        PyVarObject_HEAD_INIT(NULL, 0)
    #else
        PyObject_HEAD_INIT(NULL)
    0,                               /* ob_size */
    #endif
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
    /* declarations for DLL import/export */
    #ifndef PyMODINIT_FUNC
        #define PyMODINIT_FUNC void
    #endif
    PyMODINIT_FUNC initHLL(void)
#endif

{
    PyObject* m;
    #if PY_MAJOR_VERSION >= 3
        if (PyType_Ready(&HyperLogLogType) < 0) {
            return NULL;
        }

        m = PyModule_Create(&HyperLogLogmodule);

        if (m == NULL) {
            return NULL;
        }
    #else
        if (PyType_Ready(&HyperLogLogType) < 0) {
            return;
        }

        char *description = "HyperLogLog cardinality estimator.";
        m = Py_InitModule3("HLL", module_methods, description);

        if (m == NULL) {
            return;
        }
    #endif

    Py_INCREF(&HyperLogLogType);
    PyModule_AddObject(m, "HyperLogLog", (PyObject *)&HyperLogLogType);

    #if PY_MAJOR_VERSION >= 3
        return m;
    #endif
}


/* Counts leading zeros (number of consecutive of zero bits from the left) in an
 * unsigned 64bit integer. */
static inline uint8_t clz(uint64_t x) {

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

    uint8_t shift;

    /* Do a binary search to find which byte contains the first set bit. */
    if (x >= (1UL << 32UL)) {
        if (x >= (1UL << 48UL)) {
            if (x >= (1UL << 56UL)) shift = 56;
            else shift = 48;
        } else {
            if (x >= (1UL << 38UL)) shift = 40;
            else shift = 32;
        }
    } else {
        if (x >= (1U << 16U)) {
            if (x >= (1U << 24U)) shift = 24;
            else shift = 16;
        } else {
            if (x >= (1U << 8U)) shift = 8;
            else shift = 0;
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

/* ========================== Register encoding ==============================
 *
 * The value of a register will never exceed 64 so using a byte to store the
 * value is wasteful. Instead each register uses 6 bits of memory:
 *
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
 *
 * The first six bits in b0 are an unused offset. With the exception of byte
 * aligned registers (e.g. m4), registers will have bits in consecutive bytes.
 * For example, the register m2 has bits in b1 and b2. The higher order bits
 * of m2 are in b1 and the lower order bytes of m2 are in the b2.
 *
 * Getting a register
 * ------------------
 *
 * Suppose we want to get register m2 (e.g. m=2). First we determining the
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
 * Next, move the left bits into the higher order positions:
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
static inline uint64_t getReg(uint64_t m, char * regs)
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
static inline void setReg(uint64_t m, uint8_t n, char *regs)
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

void printByte(char a)
{
    for (int i = 0; i < 8; i++) {
        printf("%d", !!((a << i) & 0x80));
    }
}

void setMemoryErrorMsg(uint64_t bytes)
{
    char *msg = (char *) malloc(128 * sizeof(char));
    sprintf(msg, "Failed to allocate %lu bytes. Use a smaller value for p.", bytes);
    PyErr_SetString(PyExc_MemoryError, msg);
}

uint8_t isValidIndex(uint64_t index, uint64_t size)
{
    uint8_t valid = 1;

    if (index > size - 1) {
        char * msg = "Index exceeds the number of registers.";
        PyErr_SetString(PyExc_IndexError, msg);
        valid = 0;
    }

    return valid;
}
