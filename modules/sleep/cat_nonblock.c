/*
 * cat_nonblock.c - open a file and display its contents, but exit rather than
 * wait for input.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


#define MAX_BYTES 1024 * 4


int main(int argc, char *argv[]) {
    int fd;
    size_t bytes;
    char buffer[MAX_BYTES + 1];

    if (argc != 2) {
        printf("Usage : %s <filename>\n", argv[0]);
        puts("reads the content of a file, but doesn't wait for input");
        exit(EXIT_FAILURE);
    }

    // Open the file for reading in non-blocking mode
    fd = open(argv[1], O_RDONLY | O_NONBLOCK);
    if (fd == -1) {
        puts(errno == EAGAIN ? "Open would block" : "Open failed");
        exit(EXIT_FAILURE);
    }

    // Read the file and output its contents
    do {
        // Read characters from the file
        bytes = read(fd, buffer, MAX_BYTES);
        if (bytes == -1) {
            if (errno == EAGAIN)
                puts("Normally I'd block, but you told me not to");
            else
                puts("Another ead error");
            exit(EXIT_FAILURE);
        }

        if (bytes > 0) {
            for (int i = 0; i < bytes; i++)
                putchar(buffer[i]);
        }
        // While there are no errors and the file isn't over
    } while (bytes > 0);

    close(fd);
    return 0;
}

