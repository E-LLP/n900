/*
 * ke_runtime.h
 * Copyright (c) 2012 Christina Brooks
 *
 * Kernel runtime support.
 */

#ifndef _H_MG_KE_RUNTIME_
#define _H_MG_KE_RUNTIME_


#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>

#include <DarwinTypes.h>
#include <MachO.h>

#define KE_TYPE_UNKNOWN 0
#define KE_TYPE_ARRAY 1

/* void bzero(void *s, size_t n); */
#define bzero(ptr, sz) memset(ptr, 0, sz)

/**/
#define ke_storage_type void*
#define Boolean int

typedef struct {
	uint16_t type;
} ke_type_impl;

typedef struct {
	ke_type_impl base;

	unsigned int count;
	unsigned int capacity;
	unsigned int capacityIncrement;

	ke_storage_type* array;
} ke_array_impl;

typedef void* ke_type_t;
typedef ke_type_t ke_array_t;

/* Memory */
void* ke_alloc(size_t size);
void ke_free(void* ptr);
void* ke_realloc(void* ptr, size_t size);

/* Array */
ke_array_t ke_array_with_capacity(unsigned int capacity);
bool ke_array_init(ke_array_t arr, unsigned int capacity);
ke_storage_type ke_array_get(ke_array_t arr, unsigned int index);
bool ke_array_set_at(ke_array_t arr, unsigned int index, ke_storage_type anObject);
unsigned int ke_array_get_count(ke_array_t arr);
bool ke_array_add(ke_array_t arr, ke_storage_type anObject);

/* Port */
void ke_setup_task_port(struct task_struct* task);

#define ke_critical panic

#endif