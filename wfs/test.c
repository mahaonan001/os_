#include "wfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#define NUM_FILES 1000

// 生成随机字符串
char *generate_random_string(int length) {
    char *str = (char *)malloc((length + 1) * sizeof(char));  // +1 for '\0'
    if (str == NULL) {
        perror("Memory allocation failed");
        return NULL;
    }

    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    int charset_size = sizeof(charset) - 1;  

    srand(time(NULL) + clock());
    for (int i = 0; i < length; ++i) {
        int index = rand() % charset_size;
        str[i] = charset[index];
    }
    str[length] = '\0';  

    return str;
}
// 测试本地文件系统的性能
void test_local_filesystem() {
    clock_t start, end;
    double cpu_time_used;

    // 生成并写入文件到本地文件系统
    for (int i = 0; i < NUM_FILES; ++i) {
        char filename[20];
        sprintf(filename, "./file/file%d.txt", i);
        FILE *file = fopen(filename, "w");
        // 写入一些数据到文件
        char *data;
        data=generate_random_string(10000);
        fwrite(data, sizeof(char), 10000, file);
        free(data);
        fclose(file);
    }

    // 测量文件读取时间
    start = clock();
    for (int i = 0; i < NUM_FILES; ++i) {
        char filename[20];
        sprintf(filename, "./file/file%d.txt", i);
        FILE *file = fopen(filename, "r");
        // 从文件读取数据
        fclose(file);
    }
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Local file system: Time taken for %d file retrievals: %f seconds\n", NUM_FILES, cpu_time_used);
}

// 测试自定义文件系统的性能
void test_custom_filesystem() {
    clock_t start, end;
    double cpu_time_used;

    init_file_system();

    // 生成并写入文件到自定义文件系统
    for (int i = 0; i < NUM_FILES; ++i) {
        char filename[20];
        sprintf(filename, "./file/file%d.txt", i);
        char *data;
        data=generate_random_string(10000);
        write_file(filename, data, 10000);
        free(data);
    }

    // 测量文件读取时间
    start = clock();
    for (int i = 0; i < NUM_FILES; ++i) {
        char filename[20];
        sprintf(filename, "./file/file%d.txt", i);
        char *data = read_file(filename);
        if (data)
            free(data);
    }
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Custom file system: Time taken for %d file retrievals: %f seconds\n", NUM_FILES, cpu_time_used);

    destroy_file_system();
}

// 主函数，调用测试本地和自定义文件系统的函数
int main() {
    // test_local_filesystem();
    // test_custom_filesystem();
    // write_file("./file/file101.txt", "hello world", 11);
    // char *s =  read_file("./file/file101.txt");
    // printf("%s\n", s);
    // free(s);
    // delete_file("./file/file101.txt");
    
    while (1){
        system("clear");
        int choice;
        char filename[20];
        printf("1. Write file\n");
        printf("2. Read file\n");
        printf("3. Delete file\n");
        printf("99. Test both file systems\n");
        scanf("%d", &choice);
        switch (choice)
        {
        case 1:
            system("clear");
            printf("Enter the filename: ");
            scanf("%s", filename);
            printf("Enter the content: ");
            char context[10000];
            scanf("%s", context);
            write_file(filename, context, sizeof(context)/sizeof(char));
            continue;
        case 2:
            system("clear");
            printf("Enter the filename: ");
            scanf("%s", filename);
            char *content = read_file(filename);
            printf("%s\n", content);
            free(content);
            continue;
        case 3:
            system("clear");
            printf("Enter the filename: ");
            scanf("%s", filename);
            delete_file(filename);
            continue;
        case 99:
            system("clear");
            test_local_filesystem();
            test_custom_filesystem();
            continue;
        case -1:
            system("clear");
            destroy_file_system();  
            break;
        default:
            system("clear");    
            printf("Invalid choice\n");
            continue;
        }
    } 
  return 0;
}