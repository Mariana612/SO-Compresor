#include "Compressor.h"
#include "../Commons/Codec.h"
#include "../Commons/FileList.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_WORKERS 64

typedef struct {
    int ok;
} WorkerResult;

typedef struct {
    pid_t pid;
    int fd;
} Worker;

static int worker_count(void)
{
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    if (count < 1)
        count = 1;
    return count > MAX_WORKERS ? MAX_WORKERS : (int)count;
}

/* Espera al primer hijo que termine (no necesariamente el más antiguo), lee su
 * resultado de la pipe y libera su espacio. */
static int collect_worker(Worker workers[], int *active)
{
    WorkerResult result;
    ssize_t received;
    int status = 0, slot = 0;
    pid_t pid;

    do
        pid = waitpid(-1, &status, 0);
    while (pid < 0 && errno == EINTR);
    if (pid < 0)
        waitpid(workers[0].pid, &status, 0);
    else
        while (slot < *active - 1 && workers[slot].pid != pid)
            slot++;
    received = read(workers[slot].fd, &result, sizeof(result));
    close(workers[slot].fd);
    workers[slot] = workers[--(*active)];
    return received == (ssize_t)sizeof(result) && result.ok &&
           WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS;
}

int compress_directory(const char *directory)
{
    FileList files;
    Worker workers[MAX_WORKERS];
    size_t next = 0;
    int active = 0, success = 1, limit = worker_count();

    if (!file_list_load(directory, 0, &files))
        return 0;
    while (next < files.count || active > 0) {
        if (active < limit && next < files.count) {
            int fds[2];
            if (pipe(fds) == 0) {
                pid_t pid;
                fflush(stdout);
                pid = fork();
                if (pid == 0) {
                    WorkerResult result;
                    int index;
                    close(fds[0]);
                    for (index = 0; index < active; index++)
                        close(workers[index].fd);
                    result.ok = common_compress_file(files.paths[next]);
                    fflush(stdout);
                    (void)write(fds[1], &result, sizeof(result));
                    close(fds[1]);
                    _exit(result.ok ? EXIT_SUCCESS : EXIT_FAILURE);
                }
                close(fds[1]);
                if (pid > 0) {
                    workers[active].pid = pid;
                    workers[active].fd = fds[0];
                    active++;
                    next++;
                    continue;
                }
                close(fds[0]);
            }
            if (active == 0) {
                success = 0;
                break;
            }
        }
        if (!collect_worker(workers, &active))
            success = 0;
    }
    file_list_free(&files);
    return success;
}
