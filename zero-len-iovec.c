/*
 * Minimal reproducer: a zero-length readv()/writev() still has to validate
 * the file descriptor.
 *
 * Linux checks the file mode before it looks at the vector, so on a
 * descriptor opened the wrong way an empty vector fails with EBADF.
 * qemu-user instead returns 0 without ever entering the kernel, because
 * lock_iovec() in linux-user/syscall.c returns NULL for a zero count and
 * the caller turns that into a successful return.
 *
 * Exit status: 0 when the kernel behaviour is observed, 1 otherwise.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>

static int check(const char *what, int fd, int writing)
{
    long r;

    errno = 0;
    r = writing ? writev(fd, NULL, 0) : readv(fd, NULL, 0);

    if (r == -1) {
        printf("%-24s -> -1 %s\n", what, strerror(errno));
        return errno == EBADF;
    }
    printf("%-24s -> %ld (no error)\n", what, r);
    return 0;
}

int main(void)
{
    int ro = open("/dev/null", O_RDONLY);
    int wo = open("/dev/null", O_WRONLY);
    int ok;

    if (ro < 0 || wo < 0) {
        perror("open");
        return 2;
    }

    ok = check("readv(wo_fd, NULL, 0)", wo, 0);
    ok &= check("writev(ro_fd, NULL, 0)", ro, 1);

    printf("%s\n", ok ? "PASS: the fd was validated"
                      : "FAIL: the zero-length vector skipped the fd check");
    return ok ? 0 : 1;
}
