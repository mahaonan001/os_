#ifndef WFS_H
#define WFS_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define HASH_TABLE_SIZE 1024
#define CACHE_SIZE 4

typedef struct {
    char filename[256];  // 文件名
    long offset;         // 文件在数据文件中的偏移量
    size_t size;         // 文件大小
} IndexEntry;

typedef struct {
    IndexEntry *entries[HASH_TABLE_SIZE];
    pthread_rwlock_t rwlock;
} HashTable;

typedef struct {
    char filename[256];
    char *data;
    size_t size;
    int valid;
} CacheEntry;

void init_file_system();
void destroy_file_system();
void write_file(const char *filename, const char *data, size_t size);
char* read_file(const char *filename);
void delete_file(const char *filename);
IndexEntry* find_index_entry(HashTable *hash_table, const char *filename);
unsigned int hash(const char *filename);
void insert_index_entry(HashTable *hash_table, const char *filename, long offset, size_t size);
#endif // WFS_H
