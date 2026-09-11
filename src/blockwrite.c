#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define BLOCK_SIZE (4 * 1024 * 1024) // 4 MiB = 4,194,304 bytes
#define ALIGNMENT  4096              // 4 KiB alignment for O_DIRECT

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <device_path>\n", argv[0]);
        fprintf(stderr, "Example: %s /dev/nst0\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *device_path = argv[1];

    // Open target device node directly with O_DIRECT and O_WRONLY
    int out_fd = open(device_path, O_WRONLY | O_DIRECT);
    if (out_fd < 0) {
        perror("Failed to open device node with O_DIRECT");
        return EXIT_FAILURE;
    }

    // Allocate 4 MiB buffer aligned on a 4 KiB boundary
    char *buffer = NULL;
    if (posix_memalign((void **)&buffer, ALIGNMENT, BLOCK_SIZE) != 0) {
        perror("Failed to allocate memory");
        close(out_fd);
        return EXIT_FAILURE;
    }

    size_t total_read = 0;

    while (1) {
        ssize_t bytes_read = read(STDIN_FILENO, buffer + total_read, BLOCK_SIZE - total_read);

        if (bytes_read > 0) {
            total_read += (size_t)bytes_read;

            // Full 4 MiB block ready to write
            if (total_read == BLOCK_SIZE) {
                ssize_t total_written = 0;
                while (total_written < BLOCK_SIZE) {
                    ssize_t bytes_written = write(out_fd, 
                                                  buffer + total_written, 
                                                  BLOCK_SIZE - total_written);
                    if (bytes_written <= 0) {
                        if (bytes_written < 0 && errno == EINTR) {
                            continue;
                        }
                        perror("Error writing to device");
                        free(buffer);
                        close(out_fd);
                        return EXIT_FAILURE;
                    }
                    total_written += bytes_written;
                }
                total_read = 0; // Reset buffer counter
            }
        } else if (bytes_read == 0) {
            // EOF reached on stdin
            if (total_read > 0) {
                // Zero-fill remaining space to ensure the final block is exactly 4 MiB
                for (size_t i = total_read; i < BLOCK_SIZE; i++) {
                    buffer[i] = 0;
                }

                ssize_t total_written = 0;
                while (total_written < BLOCK_SIZE) {
                    ssize_t bytes_written = write(out_fd, 
                                                  buffer + total_written, 
                                                  BLOCK_SIZE - total_written);
                    if (bytes_written <= 0) {
                        if (bytes_written < 0 && errno == EINTR) {
                            continue;
                        }
                        perror("Error writing final block to device");
                        free(buffer);
                        close(out_fd);
                        return EXIT_FAILURE;
                    }
                    total_written += bytes_written;
                }
            }
            break;
        } else {
            if (errno == EINTR) {
                continue;
            }
            perror("Error reading from stdin");
            free(buffer);
            close(out_fd);
            return EXIT_FAILURE;
        }
    }

    free(buffer);
    close(out_fd);
    return EXIT_SUCCESS;
}
