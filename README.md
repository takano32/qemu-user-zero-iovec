# qemu-user skips the file descriptor check for a zero-length `readv()`/`writev()`

Linux validates the file descriptor **before** it looks at the vector, so an
empty vector on a descriptor opened the wrong way fails with `EBADF`:

```c
int fd = open("/dev/null", O_WRONLY);

readv(fd, NULL, 0);     /* kernel: -1 EBADF */
```

qemu-user returns `0` instead, without ever entering the host kernel.

## Reproducer

```sh
make
./zero-len-iovec                      # native: PASS, exit 0
qemu-aarch64 ./zero-len-iovec         # qemu-user: FAIL, exit 1
```

Measured on an aarch64 host, 2026-10-25:

```
$ ./zero-len-iovec
readv(wo_fd, NULL, 0)    -> -1 Bad file descriptor
writev(ro_fd, NULL, 0)   -> -1 Bad file descriptor
PASS: the fd was validated

$ qemu-aarch64 ./zero-len-iovec        # qemu 10.2.1 (distro package)
readv(wo_fd, NULL, 0)    -> 0 (no error)
writev(ro_fd, NULL, 0)   -> 0 (no error)
FAIL: the zero-length vector skipped the fd check
```

The same happens with `qemu-x86_64` and an x86-64 guest:

```sh
make clean && make CC=x86_64-linux-gnu-gcc
qemu-x86_64 ./zero-len-iovec
```

## Root cause

`lock_iovec()` in `linux-user/syscall.c` returns `NULL` with `errno` left at
zero when the vector count is zero, and every caller translates a `NULL`
vector with a zero `errno` into "0 bytes transferred":

```c
if (count == 0) {
    errno = 0;
    return NULL;
}
```

The shortcut goes back to `f287b2c2d4` ("linux-user: Perform more checks on
iovec lists", 2012). It is still present on git master `d731492832`
(2026-10-24).

Affected syscalls, all of which share `lock_iovec()`:
`readv`, `writev`, `preadv`, `pwritev`, `preadv2`, `pwritev2`, `vmsplice`.

## Fix

Returning an empty vector for a zero count makes the caller issue the
syscall, and the host kernel then reports the real error:

```c
if (count == 0) {
    /*
     * The kernel validates the file descriptor before it looks at the
     * vector, so readv(fd, NULL, 0) on a read-only fd fails with EBADF
     * rather than returning 0.  Hand back an empty vector so that the
     * caller issues the syscall and the host reports the real error.
     */
    return g_new0(struct iovec, 1);
}
```

With that change this reproducer passes under both `qemu-aarch64` and
`qemu-x86_64`, and the tcg suites stay green: aarch64-linux-user 106/106,
x86_64-linux-user 73/73 (including a new
`tests/tcg/multiarch/linux/linux-zero-len-iovec.c`).

The patch is prepared on branch `iovec-zero-count` on top of `d731492832`.
