//
// =========
// 1lang1server/c-server/DataStructures
//
// =========
// 
//
// Created by Samandar Komil on 18/04/2025.
// 
//
// 
// dynamic_array.c


typedef struct DynamicArray {
    int *array;
    int len;
    int capacity;

    void (*append)(struct DynamicArray *self, int value);
    void (*insert)(struct DynamicArray *self, int index, int value);
    int (*pop)(struct DynamicArray *self);
    int (*remove)(struct DynamicArray *self, int index);
} DynamicArray;


// Prototypes

DynamicArray dynamic_array_init(int capacity);

void append(DynamicArray *self, int value);
void insert(DynamicArray *self, int index, int value);
int pop(DynamicArray *self);
int remove(DynamicArray *self, int index);