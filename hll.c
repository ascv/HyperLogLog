#include <Python.h>
#include "structmember.h"
#include "hll.h"
#include "murmur3.h"
#include <math.h>
#include <ctype.h>
#include <stdint.h>

typedef struct {
    PyObject_HEAD
    short int k;
    uint32_t size;
    char * registers;

} HyperLogLog;

static void
HyperLogLog_dealloc(HyperLogLog* self)
{
    self->ob_type->tp_free((PyObject*)self);
}

static PyObject *
HyperLogLog_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    HyperLogLog *self;
    self = (HyperLogLog *)type->tp_alloc(type, 0);

    if (!PyArg_ParseTuple(args, "i", &self->k))
        return NULL;

    self->size = 1 << self->k;
    self->registers = (char *)malloc(self->size * sizeof(char));
    memset(self->registers, 0, self->size);

    return (PyObject *)self;
}

static int
HyperLogLog_init(HyperLogLog *self, PyObject *args, PyObject *kwds)
{ 
    return 0; 
}

static PyMemberDef HyperLogLog_members[] = { 
    {NULL} /* Sentinel */
};

static PyObject *
HyperLogLog_add(HyperLogLog *self, PyObject * args) 
{
    const char *data;
    const uint32_t dataLength;

    if (!PyArg_ParseTuple(args, "s#", &data, &dataLength))
      return NULL; //TODO: add error

    uint32_t *hash = (uint32_t *) malloc(sizeof(uint32_t));
    uint32_t index;
    uint32_t rank;

    //TODO: change 42 to random seed
    MurmurHash3_x86_32((void *) data, dataLength, 42, (void *) hash);

    // get the first k bits as an int
    index = *hash >> (32 - self->k);

    // get the rank of the remaining 32-k bits
    rank = leadingZeroCount((*hash << self->k) >> self->k) - self->k + 1;
    
    if (rank > self->registers[index])
      self->registers[index] = rank;

    Py_INCREF(Py_None);
    return Py_None;
};

static PyObject *
HyperLogLog_cardinality(HyperLogLog *self)
{
    double alpha = 0.0;
    switch (self->size) {
        case 16:
      	  alpha = 0.673;
	  break;
        case 32:
	  alpha = 0.697;
	  break;
        case 64:
	  alpha = 0.709;
	  break;
        default:
	  alpha = 0.7213/(1.0 + 1.079/(double) self->size);
          break;
    }
  
    uint32_t i;
    uint32_t rank;
    double sum = 0.0;
    for (i = 0; i < self->size; i++) {
        rank = self->registers[i];
        sum = sum + 1.0/pow(2, rank);
    }

    static const double j = 4294967296.0;
    double estimate = alpha * (1/sum) * self->size * self->size;
    
    if (estimate <= 2.5 * self->size) {
        const uint32_t zeros = 32 - ones((int32_t) estimate);
        if (zeros != 0)
          estimate = self->size * log(self->size/zeros);
    }
    
    if (estimate <= (1.0/3.0) * j)
        estimate = estimate;

    else if (estimate > (1.0/3.0) * j)
        estimate = (-1.0 * j) * log(1.0 - estimate/j);

    return Py_BuildValue("d", estimate);
}

static PyObject *
HyperLogLog_registers(HyperLogLog *self)
{
    return Py_BuildValue("s#", self->registers, self->size);
}

static PyObject *
HyperLogLog_size(HyperLogLog* self)
{
    return Py_BuildValue("i", self->size);
}


static PyMethodDef HyperLogLog_methods[] = {
    {"add", (PyCFunction)HyperLogLog_add, METH_VARARGS, //TODO: check for single arg macro
     "Add an element to a random register."
    },
    {"cardinality", (PyCFunction)HyperLogLog_cardinality, METH_NOARGS, //TODO: check for single arg macro
     "Get the cardinality."
    },
    {"registers", (PyCFunction)HyperLogLog_registers, METH_NOARGS, 
     "Get a string copy of the registers."
     },
    {"size", (PyCFunction)HyperLogLog_size, METH_NOARGS, 
     "Returns the number of registers."
    },
    {NULL}  /* Sentinel */
};

static PyTypeObject HyperLogLogType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "HLL.HyperLogLog",         /*tp_name*/
    sizeof(HyperLogLog),             /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)HyperLogLog_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "HyperLogLog object",           /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    HyperLogLog_methods,             /* tp_methods */
    HyperLogLog_members,             /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)HyperLogLog_init,      /* tp_init */
    0,                         /* tp_alloc */
    HyperLogLog_new,                 /* tp_new */
};

static PyMethodDef module_methods[] = {
    {NULL}  /* Sentinel */
};

#ifndef PyMODINIT_FUNC	/* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC
initHLL(void) 
{
    PyObject* m;

    if (PyType_Ready(&HyperLogLogType) < 0)
        return;

    m = Py_InitModule3("HLL", module_methods,
                       "HyperLogLog cardinality estimator.");

    if (m == NULL)
      return;

    Py_INCREF(&HyperLogLogType);
    PyModule_AddObject(m, "HyperLogLog", (PyObject *)&HyperLogLogType);
}

// TODO: move these to another file

/* 
 * Get the number of leading zeros in |x|.
 */
uint32_t leadingZeroCount(uint32_t x) {
  x |= (x >> 1);
  x |= (x >> 2);
  x |= (x >> 4);
  x |= (x >> 8);
  x |= (x >> 16);
  return (32 - ones(x));
}

/*
 * Get the number of bits set to 1 in |x|.
 */
uint32_t ones(uint32_t x) {
  x -= (x >> 1) & 0x55555555;
  x = ((x >> 2) & 0x33333333) + (x & 0x33333333);
  x = ((x >> 4) + x) & 0x0F0F0F0F;
  x += (x >> 8);
  x += (x >> 16);
  return(x & 0x0000003F);
}
