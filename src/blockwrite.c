#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define BLOCK_SIZE (4 * 1024 * 1024) // 4 MiB = 4,194,304 bytes
#define ALIGNMENT  4096              // 4 KiB alignment for O_DIRECT

// Write one full block in a single write() call.
// On tape every write() becomes one record, and with O_DIRECT the rest of a
// partial write would start on an unaligned address, so a short write is an
// error and is never continued.
static int write_block(int out_fd, const char *buffer, const char *what) {
    while (1) {
        ssize_t bytes_written = write(out_fd, buffer, BLOCK_SIZE);

        if (bytes_written == BLOCK_SIZE) {
            return 0;
        }

        if (bytes_written < 0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "Error writing %s to device: %s\n", what, strerror(errno));
            return -1;
        }

        fprintf(stderr, "Error writing %s to device: short write of %zd of %d bytes\n",
                what, bytes_written, BLOCK_SIZE);
        return -1;
    }
}

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
    int alloc_error = posix_memalign((void **)&buffer, ALIGNMENT, BLOCK_SIZE);
    if (alloc_error != 0) {
        fprintf(stderr, "Failed to allocate memory: %s\n", strerror(alloc_error));
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
                if (write_block(out_fd, buffer, "block") != 0) {
                    free(buffer);
                    close(out_fd);
                    return EXIT_FAILURE;
                }
                total_read = 0; // Reset buffer counter
            }
        } else if (bytes_read == 0) {
            // EOF reached on stdin
            if (total_read > 0) {
                // Zero-fill remaining space to ensure the final block is exactly 4 MiB
                memset(buffer + total_read, 0, BLOCK_SIZE - total_read);

                if (write_block(out_fd, buffer, "final block") != 0) {
                    free(buffer);
                    close(out_fd);
                    return EXIT_FAILURE;
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

    // Closing a tape device writes the filemark, so a failure here means the
    // archive on the carrier is incomplete.
    if (close(out_fd) != 0) {
        perror("Error closing device");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
