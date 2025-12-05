#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "pcgpmem.h"

void arenaInit(Arena* a, void* mem, size_t size) {
	a->start = mem;
	a->end = (void*)((uintptr_t)a->start + size);
	a->ptr = a->start;
}

void* arenaAlloc(Arena* a, size_t size) {
	uintptr_t ptr = (uintptr_t)a->ptr;
	uintptr_t aptr = (uintptr_t)ptr;
	uintptr_t mod = ptr & (PCGPMEM_ALIGN - 1);
	if (!mod) {
		aptr += PCGPMEM_ALIGN - mod;
	}
	ptr = aptr + size;
	assert(ptr <= (uintptr_t)a->end);
	a->ptr = (void*)ptr;
	return (void*)aptr;
}

void arenaFree(Arena* a) {
	a->ptr = a->start;
}




void VectorInt_init(VectorInt* vec) {
	vec->data = NULL;
	vec->size = 0;
	vec->capacity = 0;
}

void VectorInt_free(VectorInt* vec) {
	free(vec->data);
	vec->data = NULL;
	vec->size = 0;
	vec->capacity = 0;
}

void VectorInt_resize(VectorInt* vec, int new_capacity) {
	int* new_data = (int*)realloc(vec->data, new_capacity * sizeof(int));
	if (new_data == NULL) {
		fprintf(stderr, "Failed to allocate memory\n");
		exit(1);
	}
	vec->data = new_data;
	vec->capacity = new_capacity;
}

void VectorInt_push_back(VectorInt* vec, int value) {
	if (vec->size >= vec->capacity) {
		int new_capacity = vec->capacity == 0 ? 1 : vec->capacity * 2;
		VectorInt_resize(vec, new_capacity);
	}
	vec->data[vec->size++] = value;
}

void VectorInt_clear(VectorInt* vec) {
	vec->size = 0;
}