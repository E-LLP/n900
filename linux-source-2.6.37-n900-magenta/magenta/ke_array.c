/*
 * ke_array.c
 * Copyright (c) 2012 Christina Brooks
 *
 * Kernel array.
 */

 #include "ke_runtime.h"

 #define RefToImpl() ke_array_impl* impl = (ke_array_impl*)arr
 #define HaveUpdated() /**/
 #define RetainType(tt) /**/
 #define ReleaseType(tt) /**/

bool ke_array_init(ke_array_t arr, unsigned int capacity)
{
	RefToImpl();
	size_t size = sizeof(ke_storage_type) * capacity;

	impl->array = ke_alloc(size);
	if (!impl->array) {
		return false;
	}

	impl->base.type = KE_TYPE_ARRAY;

	impl->capacity = capacity;
	impl->capacityIncrement = (capacity)? capacity : 16;
	impl->count = 0;

	memset(impl->array, 0, size);

	return true;
}

ke_array_t ke_array_with_capacity(unsigned int capacity)
{
	ke_array_impl* impl = ke_alloc(sizeof(ke_array_impl));

	ke_array_init(impl, capacity);

	return (ke_array_t)impl;
}

unsigned int ke_array_ensure_capacity(ke_array_t arr, unsigned int newCapacity)
{
	RefToImpl();
	ke_storage_type* newArray;
	int newSize;

	if (newCapacity <= impl->capacity)
	{
		return impl->capacity;
	}

	newCapacity = (((newCapacity - 1) / impl->capacityIncrement) + 1)
                * impl->capacityIncrement;
    newSize = sizeof(ke_storage_type) * newCapacity;

    newArray = ke_realloc(impl->array, newSize);

    if (!newArray) {
    	/* we're fucked */
    	ke_critical("ke_array_ensure_capacity(): reallocation failed!");
    }
    else {
    	/* success */
    	impl->capacity = newCapacity;
    	impl->array = newArray;
    }

    return impl->capacity;
}

ke_storage_type ke_array_get(ke_array_t arr, unsigned int index)
{
	RefToImpl();

	if (index >= impl->count)
	{
		/* Out of bounds */
        return (ke_storage_type)0;
    }
    else
    {
    	/* In bounds, so return */
        return (ke_storage_type)impl->array[index];
    }
}

unsigned int ke_array_get_count(ke_array_t arr)
{
	RefToImpl();
	return impl->count;
}

bool ke_array_set_at(ke_array_t arr, unsigned int index, ke_storage_type anObject)
{
	RefToImpl();

	unsigned int i;
	unsigned int newCount = impl->count + 1;

	if ((index > impl->count) || !anObject)
		return false;

	// do we need more space?
	if (newCount > impl->capacity && newCount > ke_array_ensure_capacity(arr, newCount))
		return false;

	HaveUpdated();

	if (index != impl->count) {
		for (i = impl->count; i > index; i--) {
			impl->array[i] = impl->array[i-1];
		}
	}

	impl->array[index] = anObject;
	RetainType(anObject);
	impl->count += 1;

	return true;
}

bool ke_array_add(ke_array_t arr, ke_storage_type anObject)
{
	return ke_array_set_at(arr, ke_array_get_count(arr), anObject);
}