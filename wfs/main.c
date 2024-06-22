#include "wfs.h"

int main() {
    init_file_system();

    // 写入文件
    write_file("test1.txt", "Hello, World!", 13);
    write_file("test2.txt", "Welcome to WFS!", 15);

    // 读取文件
    char *data1 = read_file("test1.txt");
    if (data1) {
        printf("Read test1.txt: %s\n", data1);
        free(data1);
    }

    char *data2 = read_file("test2.txt");
    if (data2) {
        printf("Read test2.txt: %s\n", data2);
        free(data2);
    }

    // 删除文件
    delete_file("test1.txt");
    data1 = read_file("test1.txt");
    if (data1 == NULL) {
        printf("test1.txt successfully deleted\n");
    }
    delete_file("test2.txt");
    destroy_file_system();
    return 0;
}
