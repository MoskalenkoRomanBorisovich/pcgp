#ifndef PCGPMEM_H
#define PCGPMEM_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/// @brief memory alignment size
#define PCGPMEM_ALIGN (2 * sizeof(void *))

/// @brief arena alocator
typedef struct {
	void* start;
	void* end;
	void* ptr;
} Arena;

void arenaInit(Arena* a, void* mem, size_t size);
void* arenaAlloc(Arena* a, size_t size);
void arenaFree(Arena* a);

// bit array
typedef uint8_t* BitArray;
inline void bitId2pos(int bitId, int* byteId, uint8_t* pos) {
	*pos = (uint8_t)((bitId % 8) + 8) % 8;
	*byteId = (bitId - *pos) / 8;
}

inline void setBit(BitArray arr, int i) {
	uint8_t pos = 0;
	int k = 0;
	bitId2pos(i, &k, &pos);
	arr[k] |= (uint8_t)(1u << pos);
}
inline void clearBit(BitArray arr, int i) {
	uint8_t pos = 0;
	int k = 0;
	bitId2pos(i, &k, &pos);
	arr[k] &= (uint8_t)~(1u << pos);
}
inline bool getBit(const BitArray arr, int i) {
	uint8_t pos = 0;
	int k = 0;
	bitId2pos(i, &k, &pos);
	return arr[k] & (uint8_t)(1u << pos);
}

// VectorInt


typedef struct {
	int* data;
	int size;
	int capacity;
} VectorInt;

void VectorInt_init(VectorInt* vec);

void VectorInt_free(VectorInt* vec);
void VectorInt_resize(VectorInt* vec, int new_capacity);
void VectorInt_push_back(VectorInt* vec, int value);
void VectorInt_clear(VectorInt* vec);

#endif
