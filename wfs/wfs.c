#include "wfs.h"
#include <string.h>

static HashTable hash_table;
static CacheEntry cache[CACHE_SIZE];
static pthread_rwlock_t cache_lock;

// 初始化文件系统
void init_file_system() {
    pthread_rwlock_init(&hash_table.rwlock, NULL);
    pthread_rwlock_init(&cache_lock, NULL);

    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        hash_table.entries[i] = NULL;
    }

    for (int i = 0; i < CACHE_SIZE; i++) {
        cache[i].valid = 0;
    }
}

// 销毁文件系统
void destroy_file_system() {
    pthread_rwlock_destroy(&hash_table.rwlock);
    pthread_rwlock_destroy(&cache_lock);

    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        if (hash_table.entries[i] != NULL) {
            free(hash_table.entries[i]);
        }
    }
}

// 写文件
void write_file(const char *filename, const char *data, size_t size) {
    FILE *data_file = fopen("./file/all/data.wfs", "ab");
    if (data_file == NULL) {
        perror("Failed to open data file");
        return;
    }

    fseek(data_file, 0, SEEK_END);
    long offset = ftell(data_file);
    fwrite(data, sizeof(char), size, data_file);
    fclose(data_file);

    pthread_rwlock_wrlock(&hash_table.rwlock);
    insert_index_entry(&hash_table, filename, offset, size);
    pthread_rwlock_unlock(&hash_table.rwlock);
}

// 读文件
char* read_file(const char *filename) {
    IndexEntry *entry;

    pthread_rwlock_rdlock(&hash_table.rwlock);
    entry = find_index_entry(&hash_table, filename);
    pthread_rwlock_unlock(&hash_table.rwlock);

    if (entry == NULL) {
        printf("File not found: %s\n", filename);
        return NULL;
    }

    pthread_rwlock_rdlock(&cache_lock);
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].valid && strcmp(cache[i].filename, filename) == 0) {
            pthread_rwlock_unlock(&cache_lock);
            return cache[i].data;
        }
    }
    pthread_rwlock_unlock(&cache_lock);

    FILE *data_file = fopen("./file/all/data.wfs", "rb");
    if (data_file == NULL) {
        printf("Failed to open data file\n");
        return NULL;
    }

    fseek(data_file, entry->offset, SEEK_SET);
    char *data = (char *)malloc(entry->size + 1);
    fread(data, sizeof(char), entry->size, data_file);
    data[entry->size] = '\0';
    fclose(data_file);

    // Update cache
    pthread_rwlock_wrlock(&cache_lock);
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (!cache[i].valid) {
            strncpy(cache[i].filename, filename, 256);
            cache[i].data = data;
            cache[i].size = entry->size;
            cache[i].valid = 1;
            break;
        }
    }
    pthread_rwlock_unlock(&cache_lock);

    return data;
}

// 删除文件
void delete_file(const char *filename) {
    pthread_rwlock_wrlock(&hash_table.rwlock);
    IndexEntry *entry = find_index_entry(&hash_table, filename);
    if (entry != NULL) {
        free(hash_table.entries[hash(filename)]);
        hash_table.entries[hash(filename)] = NULL;
    }
    pthread_rwlock_unlock(&hash_table.rwlock);

    pthread_rwlock_wrlock(&cache_lock);
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].valid && strcmp(cache[i].filename, filename) == 0) {
            free(cache[i].data);  // Free cached data
            cache[i].data = NULL;  // Reset cached data pointer
            cache[i].valid = 0;
            break;
        }
    }
    pthread_rwlock_unlock(&cache_lock);
}

// 插入索引条目
void insert_index_entry(HashTable *hash_table, const char *filename, long offset, size_t size) {
    unsigned int index = hash(filename);
    IndexEntry *entry = (IndexEntry *)malloc(sizeof(IndexEntry));
    strncpy(entry->filename, filename, 256);
    entry->offset = offset;
    entry->size = size;
    hash_table->entries[index] = entry;
}

// 查找索引条目
IndexEntry* find_index_entry(HashTable *hash_table, const char *filename) {
    unsigned int index = hash(filename);
    return hash_table->entries[index];
}

// 哈希函数
unsigned int hash(const char *filename) {
    unsigned int hash = 0;
    while (*filename) {
        hash = (hash << 5) + *filename++;
    }
    return hash % HASH_TABLE_SIZE;
}