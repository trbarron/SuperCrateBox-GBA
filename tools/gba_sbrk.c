/* Heap and syscall stubs for GBA builds with vanilla newlib.
 *
 * devkitARM gets these from libsysbase, which vanilla newlib does not ship.
 * The GBA has no files, processes or console, so everything except the heap
 * is a stub; they exist only to satisfy newlib's link-time references.
 */
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

extern char __end__;             /* end of .bss/.ewram, from gba_mb.ld */

char *fake_heap_start = &__end__;
char *fake_heap_end   = 0;       /* filled in by gba_crt0.s */

static char *heap_ptr = 0;       /* zeroed by crt0's bss clear */

caddr_t _sbrk(int incr)
{
	if (heap_ptr == 0) heap_ptr = fake_heap_start;

	char *prev = heap_ptr;
	if (fake_heap_end != 0 && heap_ptr + incr > fake_heap_end)
	{
		errno = ENOMEM;
		return (caddr_t) -1;
	}
	heap_ptr += incr;
	return (caddr_t) prev;
}

void _exit(int status)
{
	(void) status;
	for (;;) {}
}

int _kill(int pid, int sig)   { (void) pid; (void) sig; errno = EINVAL; return -1; }
int _getpid(void)             { return 1; }
int _close(int fd)            { (void) fd; errno = EBADF; return -1; }
int _isatty(int fd)           { (void) fd; errno = EBADF; return 0; }
off_t _lseek(int fd, off_t offset, int whence)
{
	(void) fd; (void) offset; (void) whence;
	errno = EBADF;
	return (off_t) -1;
}
int _read(int fd, char *buf, int len)  { (void) fd; (void) buf; (void) len; errno = EBADF; return -1; }
int _write(int fd, const char *buf, int len) { (void) fd; (void) buf; (void) len; errno = EBADF; return -1; }
int _fstat(int fd, struct stat *st)
{
	(void) fd;
	st->st_mode = S_IFCHR;
	return 0;
}
