/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Host unit tests for lib/ (string helpers, the kprintf formatter and the
 * Device Tree reader). Built and run on the development machine by tests/lib_test.sh,
 * which passes the compiled tests/fdt_test.dts as the first argument.
 */

#include "lib/fdt.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* The few host libc/POSIX functions used, declared by hand so no host headers clash with ours. */
int puts(const char *s);
int open(const char *path, int flags, ...);
long read(int fd, void *buf, unsigned long n);
int close(int fd);

#define HOST_O_RDONLY 0
#define FDT_BUF_SIZE  4096

#define BUF_SIZE 256

struct buf {
	char data[BUF_SIZE];
	usize len;
};

static int failures;

static void buf_sink(char c, void *ctx)
{
	struct buf *b = ctx;

	if (b->len < BUF_SIZE - 1)
		b->data[b->len++] = c;
	b->data[b->len] = '\0';
}

static bool str_eq(const char *a, const char *b)
{
	while (*a && *a == *b) {
		a++;
		b++;
	}
	return *a == *b;
}

static void check(bool ok, const char *what)
{
	if (!ok) {
		puts(what);
		failures++;
	}
}

static void check_fmt(const char *expect, const char *fmt, ...) ATTR_PRINTF(2, 3);

static void check_fmt(const char *expect, const char *fmt, ...)
{
	struct buf b = {.len = 0};
	va_list ap;
	int n;

	b.data[0] = '\0';
	va_start(ap, fmt);
	n = kvformat(buf_sink, &b, fmt, ap);
	va_end(ap);
	if (!str_eq(b.data, expect) || (usize)n != strlen(expect)) {
		puts("FAIL kprintf format:");
		puts(fmt);
		puts("  got:");
		puts(b.data);
		puts("  expected:");
		puts(expect);
		failures++;
	}
}

static void test_string(void)
{
	u8 a[16];
	u8 b[16];

	memset(a, 0xab, sizeof(a));
	for (usize i = 0; i < sizeof(a); i++)
		check(a[i] == 0xab, "FAIL memset fills every byte");
	check(memset(a, 0, 0) == a, "FAIL memset returns dst");

	for (usize i = 0; i < sizeof(b); i++)
		b[i] = (u8)i;
	check(memcpy(a, b, sizeof(b)) == a, "FAIL memcpy returns dst");
	for (usize i = 0; i < sizeof(a); i++)
		check(a[i] == (u8)i, "FAIL memcpy copies every byte");

	check(strlen("") == 0, "FAIL strlen empty");
	check(strlen("OS-001") == 6, "FAIL strlen");
}

static void test_kprintf(void)
{
	check_fmt("plain text", "plain text");
	check_fmt("100%", "100%%");
	check_fmt("a OS-001 z", "a %s z", "OS-001");
	/* volatile hides the constant NULL from GCC's format checker; kprintf must handle it. */
	const char *volatile null_str = NULL;
	check_fmt("(null)", "%s", null_str);
	check_fmt("[x]", "[%c]", 'x');
	check_fmt("0 42 -42", "%d %d %d", 0, 42, -42);
	check_fmt("-2147483648", "%d", (int)(-2147483647 - 1));
	check_fmt("4294967295", "%u", 4294967295u);
	check_fmt("deadbeef", "%x", 0xdeadbeefu);
	check_fmt("18446744073709551615", "%lu", (unsigned long)-1);
	check_fmt("-9223372036854775808", "%ld", -9223372036854775807L - 1);
	check_fmt("ffffffffffffffff", "%lx", (unsigned long)-1);
	check_fmt("0x0000000040080000", "%p", (void *)0x40080000ul);
	check_fmt("   42|00042|-0042|  -42", "%5d|%05d|%05d|%5d", 42, 42, -42, -42);
	check_fmt("0000beef", "%08x", 0xbeefu);
	check_fmt("  ab", "%4s", "ab");
}

static u8 fdt_buf[FDT_BUF_SIZE] ATTR_ALIGNED(8);

static void test_fdt(const char *path)
{
	int fd = open(path, HOST_O_RDONLY);
	long n = fd >= 0 ? read(fd, fdt_buf, sizeof(fdt_buf)) : -1;
	struct fdt_mem_region r[4];

	if (fd >= 0)
		close(fd);
	check(n > 0, "FAIL fdt: cannot read test DTB");
	if (n <= 0)
		return;

	check(fdt_valid(fdt_buf), "FAIL fdt_valid on a good DTB");
	check(fdt_total_size(fdt_buf) == (u32)n, "FAIL fdt_total_size");

	int count = fdt_memory_regions(fdt_buf, r, 4);

	check(count == 3, "FAIL fdt_memory_regions count (expected 3)");
	if (count == 3) {
		check(r[0].base == 0x0 && r[0].size == 0x3c000000, "FAIL fdt region 0");
		check(r[1].base == 0x40000000 && r[1].size == 0x100000000, "FAIL fdt region 1");
		check(r[2].base == 0x200000000 && r[2].size == 0x10000000, "FAIL fdt region 2");
	}
	check(fdt_memory_regions(fdt_buf, r, 1) == 1, "FAIL fdt_memory_regions respects max");

	/* A damaged header must be rejected, not parsed. */
	fdt_buf[0] ^= 0xff;
	check(!fdt_valid(fdt_buf), "FAIL fdt_valid accepts a bad magic");
	check(fdt_memory_regions(fdt_buf, r, 4) == -1, "FAIL fdt_memory_regions on bad magic");
	fdt_buf[0] ^= 0xff;
	check(!fdt_valid(NULL), "FAIL fdt_valid(NULL)");
}

int main(int argc, char **argv)
{
	test_string();
	test_kprintf();
	if (argc > 1)
		test_fdt(argv[1]);
	else
		check(false, "FAIL lib_test: no test DTB given");
	if (failures) {
		puts("lib_test: FAILED");
		return 1;
	}
	puts("lib_test: all passed");
	return 0;
}
