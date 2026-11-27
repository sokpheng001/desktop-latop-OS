/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Host unit tests for lib/ (string helpers, the kprintf formatter and the
 * Device Tree reader), the physical page allocator (kernel/pmm.c) and the kernel
 * heap (kernel/heap.c). Built and
 * run on the development machine by tests/lib_test.sh, which passes the compiled
 * tests/fdt_test.dts as the first argument.
 */

#include "lib/fdt.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "heap.h"
#include "pmm.h"

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

	count = fdt_reserved_regions(fdt_buf, r, 4);
	check(count == 2, "FAIL fdt_reserved_regions count (expected 2)");
	if (count == 2) {
		check(r[0].base == 0x0 && r[0].size == 0x1000, "FAIL fdt reserved 0");
		check(r[1].base == 0x100200000 && r[1].size == 0x400000, "FAIL fdt reserved 1");
	}
	check(fdt_reserved_regions(fdt_buf, r, 1) == 2,
	      "FAIL fdt_reserved_regions reports every entry even past max");

	struct fdt_device dev[4];

	count = fdt_find_compatible(fdt_buf, "virtio,mmio", dev, 4);
	check(count == 2, "FAIL fdt_find_compatible count (expected 2: one disabled, one other)");
	if (count == 2) {
		check(dev[0].base == 0x0a000000 && dev[0].size == 0x200 && dev[0].irq_cells == 3 &&
			  dev[0].irq[0] == 0 && dev[0].irq[1] == 0x10 && dev[0].irq[2] == 1,
		      "FAIL fdt device 0 (root cells, three interrupt cells)");
		check(dev[1].base == 0x1000 && dev[1].size == 0x200 && dev[1].irq_cells == 1 &&
			  dev[1].irq[0] == 5,
		      "FAIL fdt device 1 (bus cells, second compatible, has a child)");
	}
	check(fdt_find_compatible(fdt_buf, "virtio,mmio", dev, 1) == 2,
	      "FAIL fdt_find_compatible reports every device even past max");
	check(fdt_find_compatible(fdt_buf, "virtio", dev, 4) == 0,
	      "FAIL fdt_find_compatible matches a prefix");

	/* A damaged header must be rejected, not parsed. */
	fdt_buf[0] ^= 0xff;
	check(!fdt_valid(fdt_buf), "FAIL fdt_valid accepts a bad magic");
	check(fdt_memory_regions(fdt_buf, r, 4) == -1, "FAIL fdt_memory_regions on bad magic");
	check(fdt_reserved_regions(fdt_buf, r, 4) == -1, "FAIL fdt_reserved_regions on bad magic");
	check(fdt_find_compatible(fdt_buf, "virtio,mmio", dev, 4) == -1,
	      "FAIL fdt_find_compatible on bad magic");
	fdt_buf[0] ^= 0xff;
	check(!fdt_valid(NULL), "FAIL fdt_valid(NULL)");
}

/*
 * Fake physical memory for the page allocator: page-aligned host memory stands in for
 * RAM, so the allocator's bitmap lands in it just as it does on a board.
 */
#define PMM_TEST_PAGES 80u
static u8 pmm_ram[PMM_TEST_PAGES * PMM_PAGE_SIZE] ATTR_ALIGNED(PMM_PAGE_SIZE);

static uptr pg(usize i)
{
	return (uptr)pmm_ram + i * PMM_PAGE_SIZE;
}

/*
 * Layout: RAM 0 = pages 1-19 (its unaligned start loses page 0), a gap at 20-23,
 * RAM 1 = pages 24-79. Reserved: pages 2-3, an unaligned range touching 30-31, and a
 * range outside RAM. The bitmap (16 bytes, one page) must land in page 1, the first
 * free page, so the free pages are 4-19, 24-29 and 32-79: 70 of 75.
 */
static void test_pmm(void)
{
	const struct pmm_range ram[] = {
	    {pg(0) + 100, pg(20) - pg(0) - 100},
	    {pg(24), pg(80) - pg(24)},
	};
	const struct pmm_range reserved[] = {
	    {pg(2), 2 * PMM_PAGE_SIZE},
	    {pg(30) + 0x800, 0x1000},
	    {pg(90), PMM_PAGE_SIZE},
	};
	static bool seen[PMM_TEST_PAGES];
	usize taken = 0;
	bool all_ok = true;
	uptr p;

	check(!pmm_init(ram, 0, reserved, 0), "FAIL pmm_init accepts no RAM");
	const struct pmm_range all_reserved[] = {{pg(0), pg(80) - pg(0)}};
	check(!pmm_init(ram, 2, all_reserved, 1), "FAIL pmm_init with no room for its bitmap");

	check(pmm_init(ram, 2, reserved, 3), "FAIL pmm_init");
	check(pmm_total_count() == 75, "FAIL pmm total pages (expected 75)");
	check(pmm_free_count() == 70, "FAIL pmm free pages (expected 70)");

	/* Every free page exactly once, and nothing reserved, used or outside RAM. */
	while ((p = pmm_alloc_page()) != 0) {
		usize i = (p - pg(0)) / PMM_PAGE_SIZE;
		bool in_free = (i >= 4 && i <= 19) || (i >= 24 && i <= 29) || (i >= 32 && i <= 79);

		all_ok = all_ok && p >= pg(0) && !(p & (PMM_PAGE_SIZE - 1)) && in_free && !seen[i];
		if (i < PMM_TEST_PAGES)
			seen[i] = true;
		if (++taken > 75)
			break;
	}
	check(all_ok, "FAIL pmm_alloc_page returned a bad or repeated page");
	check(taken == 70 && pmm_free_count() == 0, "FAIL pmm_alloc_page did not take all 70");
	check(pmm_alloc_pages(1) == 0, "FAIL pmm_alloc_page when empty");

	for (usize i = 0; i < PMM_TEST_PAGES; i++)
		if (seen[i])
			all_ok = all_ok && pmm_free_page(pg(i));
	check(all_ok && pmm_free_count() == 70, "FAIL pmm_free_page of every page");

	/* Bad frees change nothing. */
	check(!pmm_free_page(pg(5)), "FAIL pmm double free accepted");
	check(!pmm_free_page(pg(21)), "FAIL pmm free of a page outside RAM accepted");
	check(!pmm_free_page(pg(5) + 8), "FAIL pmm misaligned free accepted");
	check(!pmm_free_pages(pg(5), 0), "FAIL pmm free of 0 pages accepted");
	p = pmm_alloc_page();
	check(p != 0 && !pmm_free_pages(p, 2) && pmm_free_page(p),
	      "FAIL pmm partial double free changed state");
	check(pmm_free_count() == 70, "FAIL pmm free count after bad frees");

	/* Runs must be contiguous, inside one RAM range and clear of reserved pages. */
	check(pmm_alloc_pages(40) == pg(32), "FAIL pmm_alloc_pages(40) (expected pages 32-71)");
	check(pmm_alloc_pages(17) == 0, "FAIL pmm_alloc_pages(17) found a run that does not exist");
	check(pmm_alloc_pages(16) == pg(4), "FAIL pmm_alloc_pages(16) (expected pages 4-19)");
	check(pmm_free_pages(pg(32), 40) && pmm_free_pages(pg(4), 16) && pmm_free_count() == 70,
	      "FAIL pmm_free_pages of runs");
	check(pmm_alloc_pages(71) == 0, "FAIL pmm_alloc_pages larger than free memory");
}

/* The heap on the same fake RAM: one range of 79 pages (1-79), nothing reserved. */
static void test_heap(void)
{
	const struct pmm_range ram[] = {{pg(1), pg(80) - pg(1)}};
	static void *objs[PMM_TEST_PAGES * 2];

	check(pmm_init(ram, 1, NULL, 0), "FAIL heap: pmm_init");
	heap_init();
	usize free0 = pmm_free_count();

	check(kmalloc(0) == NULL, "FAIL kmalloc(0) is not NULL");

	/* Same-size blocks are distinct, aligned and inside RAM; a freed block is reused first. */
	void *a = kmalloc(16);
	void *b = kmalloc(16);

	check(a && b && a != b, "FAIL kmalloc returns distinct blocks");
	check((uptr)a % HEAP_ALIGN == 0 && (uptr)b % HEAP_ALIGN == 0, "FAIL kmalloc alignment");
	check((uptr)a >= pg(1) && (uptr)a < pg(80), "FAIL kmalloc block outside RAM");
	check(heap_page_count() == 1 && heap_live_count() == 2, "FAIL heap counts after 2 small");
	kfree(a);
	check(kmalloc(10) == a, "FAIL kmalloc does not reuse a freed block of its class");
	kfree(a);
	kfree(b);

	/* Two blocks per page in the 2048 class (2032 usable); 2033 bytes needs a page of its own.
	 */
	void *c1 = kmalloc(2032);
	void *c2 = kmalloc(2032);

	uptr gap = (uptr)c1 > (uptr)c2 ? (uptr)c1 - (uptr)c2 : (uptr)c2 - (uptr)c1;

	check(c1 && c2 && gap == 2048, "FAIL 2048-byte class layout");
	check(heap_page_count() == 2, "FAIL 2048-byte class uses one page for two blocks");
	void *big = kmalloc(2033);

	check(big && heap_page_count() == 3, "FAIL kmalloc(2033) takes one whole page");
	kfree(big);
	check(heap_page_count() == 2 && pmm_free_count() == free0 - 2,
	      "FAIL kfree of a page allocation returns the page");
	kfree(c1);
	kfree(c2);

	/* kzalloc zeroes memory that held data before. */
	u8 *d = kmalloc(3 * PMM_PAGE_SIZE);

	for (usize i = 0; d && i < 3 * PMM_PAGE_SIZE; i++)
		d[i] = 0xaa;
	kfree(d);
	u8 *z = kzalloc(3 * PMM_PAGE_SIZE);
	bool zero = z != NULL;

	for (usize i = 0; z && i < 3 * PMM_PAGE_SIZE; i++)
		zero = zero && z[i] == 0;
	check(zero, "FAIL kzalloc returns zeroed memory");
	kfree(z);

	/* Bad frees are refused and counted, and change nothing. */
	usize live = heap_live_count();

	kfree(NULL);
	kfree(a);
	kfree((u8 *)b + 1);
	check(heap_bad_frees() == 2 && heap_live_count() == live, "FAIL kfree refuses bad frees");

	/* Exhaustion gives NULL, and freeing everything makes the memory usable again. */
	usize n = 0;

	while (n < ARRAY_SIZE(objs) && (objs[n] = kmalloc(3000)) != NULL)
		n++;
	check(n > 0 && n < ARRAY_SIZE(objs), "FAIL kmalloc returns NULL when memory runs out");
	for (usize i = 0; i < n; i++)
		kfree(objs[i]);
	check(kmalloc(3000) != NULL, "FAIL memory is usable again after kfree");
}

int main(int argc, char **argv)
{
	test_string();
	test_kprintf();
	test_pmm();
	test_heap();
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
