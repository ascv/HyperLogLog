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
    char * registers;       /* Contains the first set bit positions */
    unsigned short p;       /* 2^p = size */
    uint64_t * histogram;   /* register histogram */
    uint32_t seed;          /* Murmur2 hash seed */
    uint64_t size;          /* number of registers */
    double cache;           /* cached cardinality cardinality estimate */
    bool isCached;          /* 1 if the cache is up to date, otherwise 0 */
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
    self->seed = 314; //TODO: move this to init
    return (PyObject *)self;
}

static int
HyperLogLog_init(HyperLogLog *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"p", "seed", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "i|i", kwlist,
                                     &self->p, &self->seed)) {
        return -1;
    }

    if (self->p < 2 || self->p > 63) {
        char * msg = "p should be between 2 and 31";
        PyErr_SetString(PyExc_ValueError, msg);
        return -1;
    }

    self->size = 1UL << self->p;
    self->registers = (char *)calloc(self->size, sizeof(char));

    if (self->registers == NULL) {
        char *msg = (char *)malloc(128 * sizeof(char));
        sprintf(msg, "Failed to allocate %lu bytes. Use a smaller value for p.", self->size * sizeof(char));
        PyErr_SetString(PyExc_MemoryError, msg);
        return -1;
    }

    self->histogram = (uint64_t *)calloc(64, sizeof(uint64_t));
    self->histogram[0] = self->size;
    self->cache = 0.0;
    self->isCached = 0;

    return 0;
}

static PyMemberDef HyperLogLog_members[] = {
    {NULL} /* Sentinel */
};

/* Add an element */
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

    /* Use the first p bits as an index */
    index = (hash >> (64 - self->p));

    /* Use the index to pick a random register */
    fsb = self->registers[index];

    /* Get the remaining 64-p bits */
    newFsb = hash << self->p;

    /* Find the position of first set bit. For example, "0001.." has three
     * leading zeroes and a first set bit in the fourth position. We add 1
     * to handle the case where there are no leading zeroes (e.g. "101..") */
    newFsb = clz(newFsb) + 1;

    //* Update the register */
    if (newFsb > fsb) {
        self->registers[index] = newFsb;
        self->histogram[newFsb] += 1;
        self->isCached = 0;

        /* Update the register histogram */
        if (self->histogram[fsb] > 0) {
            self->histogram[fsb] -= 1;
            self->histogram[0] -= 1;
        }

        self->histogram[0] += 1;
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
};


/* Gets a cardinality estimate */
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


/* Get a Murmur2 hash of a python string, buffer or bytes (python 3.x) as an
 * unsigned integer. */
static PyObject *
HyperLogLog_murmur2_hash(HyperLogLog *self, PyObject *args)
{
    const char *data;
    const uint64_t dataLen;

    if (!PyArg_ParseTuple(args, "s#", &data, &dataLen)) {
        return NULL;
    }

    uint64_t hash = MurmurHash64A((void *) data, dataLen, self->seed);
    return Py_BuildValue("K", hash);
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

    if (hllSize != self->size) {
        //TODO: print differing number of registers
        PyErr_SetString(PyExc_ValueError, "The number of registers is not equal");
        return NULL;
    }

    Py_DECREF(size);

    PyObject *hllByteArray = PyObject_CallMethod(hll, "registers", NULL);
    char *hllRegisters = PyByteArray_AsString(hllByteArray);

    //PyObject *histogramByteArray = PyObject_CallMethod(hll, "histogram", NULL);
    //char *histogram = PyByteArray_AsString(histogramByteArray);
    //TODO: compare the histograms

    self->isCached = 0;

    uint32_t i;
    for (i = 0; i < self->size; i++) {
        if (self->registers[i] < hllRegisters[i]) {
            self->registers[i] = hllRegisters[i];
        }
    }

    Py_DECREF(hllByteArray);
    Py_INCREF(Py_None);

    return Py_None;
}

/* Support for pickling, called when HyperLogLog is serialized. */
static PyObject *
HyperLogLog_reduce(HyperLogLog *self)
{
    char *arr = (char *) malloc(self->size * sizeof(char));
    //

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


/* Gets a copy of the registers as a bytesarray. */
static PyObject *
HyperLogLog_registers(HyperLogLog *self)
{
    PyObject *registers;
    registers = PyByteArray_FromStringAndSize(self->registers, self->size);
    return registers;
}

/* Sets register at index to rank. */
static PyObject *
HyperLogLog_set_register(HyperLogLog *self, PyObject * args)
{
    const int32_t index;
    const int32_t rank;

    if (!PyArg_ParseTuple(args, "ii", &index, &rank)) {
        return NULL;
    }

    if (index < 0) {
        char * msg = "Index is negative.";
        PyErr_SetString(PyExc_ValueError, msg);
        return NULL;
    }

    if (index > self->size - 1) {
        char * msg = "Index greater than the number of registers.";
        PyErr_SetString(PyExc_IndexError, msg);
        return NULL;
    }

    if (rank >= 32) {
        char* msg = "Value greater than than the maximum possible rank 32.";
        PyErr_SetString(PyExc_ValueError, msg);
        return NULL;
    }

    if (rank < 0) {
        char * msg = "Rank is negative.";
        PyErr_SetString(PyExc_ValueError, msg);
        return NULL;
    }

    self->cache = 0;
    self->registers[index] = rank;

    Py_INCREF(Py_None);
    return Py_None;

}

/* Gets the seed value used in the Murmur hash. */
static PyObject *
HyperLogLog_seed(HyperLogLog* self)
{
    return Py_BuildValue("i", self->seed);
}

/* Sets all the registers. */
static PyObject *
HyperLogLog_set_registers(HyperLogLog *self, PyObject *args)
{
    PyObject *obj;
    char* registers;
    uint32_t len;
    int i;

    if (!PyArg_ParseTuple(args, "O", &obj)) {
        return NULL;
    }

    if (PyByteArray_Check(obj)) {
        len = PyByteArray_Size(obj);
        registers = PyByteArray_AsString(obj);
    }

    else if (PyBytes_Check(obj)) {
        len = PyBytes_Size(obj);
        registers = PyBytes_AsString(obj);
    }

    else {
        char* msg = "Registers must be a bytearray or bytes type.";
        PyErr_SetString(PyExc_TypeError, msg);
        return NULL;
    }

    if (len != self->size) {
        char msg[74];
        sprintf(msg, "Invalid size. Expected %u registers received %u registers.", self->size, len);
        PyErr_SetString(PyExc_ValueError, msg);
        return NULL;
    }

    self->use_cache = 0;

    // Check for bad register values
    for (i = 0; i < self->size; i++) {
        if (registers[i] > 32) {
            char* msg = "Value greater than than the maximum possible rank (32).";
            PyErr_SetString(PyExc_ValueError, msg);
            return NULL;
        }
    }

    // Only set registers if all values are okay
    for (i = 0; i < self->size; i++) {
        self->registers[i] = registers[i];
    }

    Py_INCREF(Py_None);
    return Py_None;
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

    //TODO: also set histogram here

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
    {"murmur2_hash", (PyCFunction)HyperLogLog_murmur2_hash, METH_VARARGS,
     "Get a Murmur2 hash"
    },
    {"__reduce__", (PyCFunction)HyperLogLog_reduce, METH_NOARGS,
     "Serialization helper function for pickling."
    },
    {"registers", (PyCFunction)HyperLogLog_registers, METH_NOARGS,
     "Get a copy of the registers as a bytearray."
    },
    {"seed", (PyCFunction)HyperLogLog_seed, METH_NOARGS,
     "Get the seed used in the Murmur2 hash."
    },
    {"set_registers", (PyCFunction)HyperLogLog_set_registers, METH_VARARGS,
     "Set the registers with a bytearray."
    },
    {"set_register", (PyCFunction)HyperLogLog_set_register, METH_VARARGS,
     "Set the register at a zero-based index to the specified rank."
    },
    {"__setstate__", (PyCFunction)HyperLogLog_set_state, METH_VARARGS,
    "De-serialization helper function for pickling."
    },
    {"size", (PyCFunction)HyperLogLog_size, METH_NOARGS,
     "Returns the number of registers."
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
        "A space efficient cardinality estimator. Version 2.0.0.",
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

    /* Do a binary search to find which byte contains the first set bit. This
     * only takes 3 operations. */
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

    /* Look up the leading zero count for (x >> shift) using this byte as an
     * index. The leading zero count for x is then: (x >> shift) - shift. */
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
