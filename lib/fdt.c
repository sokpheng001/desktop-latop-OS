/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Flattened Devicetree reader, written from the Devicetree Specification
 * (devicetree.org, release v0.4), chapter 5 "Flattened Devicetree (DTB) Format".
 * All values in a DTB are big-endian. Reads are done byte by byte, so they are
 * safe even while the MMU is off and unaligned accesses would fault.
 */

#include "lib/fdt.h"

/* Header fields, byte offsets (spec 5.2). */
#define FDT_MAGIC_VALUE	    0xd00dfeedu
#define FDT_HDR_MAGIC	    0
#define FDT_HDR_TOTALSIZE   4
#define FDT_HDR_OFF_STRUCT  8
#define FDT_HDR_OFF_STRINGS 12
#define FDT_HDR_OFF_RSVMAP  16
#define FDT_HDR_VERSION	    20
#define FDT_HDR_LAST_COMP   24
#define FDT_HDR_SIZE	    40

/* Oldest layout this reader understands: version 16 (last_comp_version <= 16). */
#define FDT_SUPPORTED_VERSION 16u

/* Structure block tokens (spec 5.4.1). */
#define FDT_BEGIN_NODE 0x1u
#define FDT_END_NODE   0x2u
#define FDT_PROP       0x3u
#define FDT_NOP	       0x4u
#define FDT_END	       0x9u

/* Defaults for #address-cells and #size-cells when the root does not set them (spec 2.3.5). */
#define FDT_DEFAULT_ADDR_CELLS 2u
#define FDT_DEFAULT_SIZE_CELLS 1u
/* reg values are at most two cells (64 bits) per address or size on our targets. */
#define FDT_MAX_CELLS	       2u

#define CELL_SIZE 4u

/* Deepest node fdt_find_compatible follows (the root is depth 1). */
#define FDT_MAX_DEPTH 16

static u32 be32(const u8 *p)
{
	return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

static u32 align4(u32 v)
{
	return (v + 3u) & ~3u;
}

static bool str_eq(const char *a, const char *b)
{
	while (*a && *a == *b) {
		a++;
		b++;
	}
	return *a == *b;
}

/* True if the stringlist value[0..len) (NUL-separated strings, spec 2.2.4) holds s. */
static bool stringlist_has(const u8 *value, u32 len, const char *s)
{
	u32 start = 0;

	for (u32 i = 0; i < len; i++) {
		if (value[i] != '\0')
			continue;
		if (str_eq((const char *)value + start, s))
			return true;
		start = i + 1;
	}
	return false;
}

/* "memory" or "memory@<unit-address>" */
static bool is_memory_node_name(const char *name)
{
	const char *m = "memory";

	while (*m && *name == *m) {
		m++;
		name++;
	}
	return *m == '\0' && (*name == '\0' || *name == '@');
}

bool fdt_valid(const void *blob)
{
	const u8 *b = blob;

	return b && be32(b + FDT_HDR_MAGIC) == FDT_MAGIC_VALUE &&
	       be32(b + FDT_HDR_LAST_COMP) <= FDT_SUPPORTED_VERSION &&
	       be32(b + FDT_HDR_VERSION) >= FDT_SUPPORTED_VERSION &&
	       be32(b + FDT_HDR_TOTALSIZE) >= FDT_HDR_SIZE;
}

u32 fdt_total_size(const void *blob)
{
	return fdt_valid(blob) ? be32((const u8 *)blob + FDT_HDR_TOTALSIZE) : 0;
}

static u64 read_cells(const u8 *p, u32 cells)
{
	u64 v = 0;

	for (u32 i = 0; i < cells; i++)
		v = (v << 32) | be32(p + i * CELL_SIZE);
	return v;
}

/* Memory reservation block entry: two big-endian 64-bit values, address then size (spec 5.3). */
#define FDT_RSV_ENTRY_SIZE 16u

int fdt_reserved_regions(const void *blob, struct fdt_mem_region *out, int max)
{
	if (!fdt_valid(blob))
		return -1;

	const u8 *b = blob;
	u32 total = be32(b + FDT_HDR_TOTALSIZE);
	u32 pos = be32(b + FDT_HDR_OFF_RSVMAP);
	int count = 0;

	/* The list ends with an entry whose address and size are both 0. */
	for (;; pos += FDT_RSV_ENTRY_SIZE) {
		if (pos + FDT_RSV_ENTRY_SIZE > total || pos + FDT_RSV_ENTRY_SIZE < pos)
			return -1;
		u64 base = read_cells(b + pos, 2);
		u64 size = read_cells(b + pos + 8, 2);

		if (base == 0 && size == 0)
			return count;
		if (count < max) {
			out[count].base = base;
			out[count].size = size;
		}
		count++;
	}
}

int fdt_memory_regions(const void *blob, struct fdt_mem_region *out, int max)
{
	if (!fdt_valid(blob))
		return -1;

	const u8 *b = blob;
	u32 total = be32(b + FDT_HDR_TOTALSIZE);
	u32 pos = be32(b + FDT_HDR_OFF_STRUCT);
	u32 strings = be32(b + FDT_HDR_OFF_STRINGS);
	u32 addr_cells = FDT_DEFAULT_ADDR_CELLS;
	u32 size_cells = FDT_DEFAULT_SIZE_CELLS;
	int depth = 0;
	int count = 0;

	/* State of the current depth-2 node (a child of the root). */
	bool in_memory = false;
	const u8 *reg = NULL;
	u32 reg_len = 0;

	if (strings >= total)
		return -1;

	for (;;) {
		if (pos + CELL_SIZE > total)
			return -1;
		u32 token = be32(b + pos);

		pos += CELL_SIZE;

		switch (token) {
		case FDT_BEGIN_NODE: {
			const char *name = (const char *)(b + pos);
			u32 len = 0;

			while (pos + len < total && name[len])
				len++;
			if (pos + len >= total)
				return -1;
			pos = align4(pos + len + 1);
			depth++;
			if (depth == 2) {
				in_memory = is_memory_node_name(name);
				reg = NULL;
				reg_len = 0;
			}
			break;
		}
		case FDT_END_NODE:
			if (depth == 2 && in_memory && reg) {
				u32 entry = (addr_cells + size_cells) * CELL_SIZE;

				for (u32 off = 0; off + entry <= reg_len && count < max;
				     off += entry) {
					out[count].base = read_cells(reg + off, addr_cells);
					out[count].size = read_cells(
					    reg + off + addr_cells * CELL_SIZE, size_cells);
					count++;
				}
			}
			if (depth == 0)
				return -1;
			depth--;
			break;
		case FDT_PROP: {
			if (pos + 2 * CELL_SIZE > total)
				return -1;
			u32 len = be32(b + pos);
			u32 nameoff = be32(b + pos + CELL_SIZE);
			const u8 *value = b + pos + 2 * CELL_SIZE;

			if (strings + nameoff >= total || pos + 2 * CELL_SIZE + len > total)
				return -1;
			const char *pname = (const char *)(b + strings + nameoff);

			/* Properties come before child nodes, so the root's cell sizes are known in
			 * time. */
			if (depth == 1 && len == CELL_SIZE && str_eq(pname, "#address-cells"))
				addr_cells = be32(value);
			else if (depth == 1 && len == CELL_SIZE && str_eq(pname, "#size-cells"))
				size_cells = be32(value);
			else if (depth == 2 && str_eq(pname, "device_type") &&
				 str_eq((const char *)value, "memory"))
				in_memory = true;
			else if (depth == 2 && str_eq(pname, "reg")) {
				reg = value;
				reg_len = len;
			}
			pos = align4(pos + 2 * CELL_SIZE + len);
			break;
		}
		case FDT_NOP:
			break;
		case FDT_END:
			return count;
		default:
			return -1;
		}

		if (addr_cells == 0 || addr_cells > FDT_MAX_CELLS || size_cells == 0 ||
		    size_cells > FDT_MAX_CELLS)
			return -1;
	}
}

/* What fdt_find_compatible tracks for each open node. */
struct fdt_node_state {
	u32 addr_cells; /* #address-cells / #size-cells this node sets for its children */
	u32 size_cells;
	bool match;
	bool enabled;
	const u8 *reg;
	u32 reg_len;
	const u8 *irq;
	u32 irq_len;
};

/* Store node n, whose parent sets cells p, as a device in d. False if reg is malformed. */
static bool fill_device(const struct fdt_node_state *n, const struct fdt_node_state *p,
			struct fdt_device *d)
{
	u32 ac = p->addr_cells, sc = p->size_cells;

	if (ac == 0 || ac > FDT_MAX_CELLS || sc > FDT_MAX_CELLS)
		return false;
	d->base = read_cells(n->reg, ac);
	d->size = sc ? read_cells(n->reg + ac * CELL_SIZE, sc) : 0;
	d->irq_cells = 0;
	for (u32 i = 0; n->irq && i < FDT_MAX_IRQ_CELLS && (i + 1) * CELL_SIZE <= n->irq_len; i++) {
		d->irq[i] = be32(n->irq + i * CELL_SIZE);
		d->irq_cells = i + 1;
	}
	return true;
}

int fdt_find_compatible(const void *blob, const char *compat, struct fdt_device *out, int max)
{
	if (!fdt_valid(blob))
		return -1;

	const u8 *b = blob;
	u32 total = be32(b + FDT_HDR_TOTALSIZE);
	u32 pos = be32(b + FDT_HDR_OFF_STRUCT);
	u32 strings = be32(b + FDT_HDR_OFF_STRINGS);
	/*
	 * node[d] is the open node at depth d; node[0] stands for the root's parent and
	 * holds the default cell sizes that apply to the root (spec 2.3.5). A node's own
	 * reg is read with its parent's cell sizes.
	 */
	struct fdt_node_state node[FDT_MAX_DEPTH + 1];
	int depth = 0;
	int count = 0;

	node[0].addr_cells = FDT_DEFAULT_ADDR_CELLS;
	node[0].size_cells = FDT_DEFAULT_SIZE_CELLS;
	if (strings >= total)
		return -1;

	for (;;) {
		if (pos + CELL_SIZE > total)
			return -1;
		u32 token = be32(b + pos);

		pos += CELL_SIZE;

		switch (token) {
		case FDT_BEGIN_NODE: {
			u32 len = 0;

			while (pos + len < total && b[pos + len])
				len++;
			if (pos + len >= total || depth == FDT_MAX_DEPTH)
				return -1;
			pos = align4(pos + len + 1);
			depth++;
			/* Cell sizes are not inherited: the defaults apply until the node says. */
			node[depth] = (struct fdt_node_state){
			    .addr_cells = FDT_DEFAULT_ADDR_CELLS,
			    .size_cells = FDT_DEFAULT_SIZE_CELLS,
			    .enabled = true,
			};
			break;
		}
		case FDT_END_NODE: {
			if (depth == 0)
				return -1;
			const struct fdt_node_state *n = &node[depth];
			const struct fdt_node_state *p = &node[depth - 1];

			if (n->match && n->enabled && n->reg &&
			    n->reg_len >= (p->addr_cells + p->size_cells) * CELL_SIZE) {
				struct fdt_device tmp;

				if (!fill_device(n, p, count < max ? &out[count] : &tmp))
					return -1;
				count++;
			}
			depth--;
			break;
		}
		case FDT_PROP: {
			if (pos + 2 * CELL_SIZE > total)
				return -1;
			u32 len = be32(b + pos);
			u32 nameoff = be32(b + pos + CELL_SIZE);
			const u8 *value = b + pos + 2 * CELL_SIZE;
			struct fdt_node_state *n = &node[depth];

			if (depth == 0 || strings + nameoff >= total ||
			    pos + 2 * CELL_SIZE + len > total)
				return -1;
			const char *pname = (const char *)(b + strings + nameoff);

			if (len == CELL_SIZE && str_eq(pname, "#address-cells"))
				n->addr_cells = be32(value);
			else if (len == CELL_SIZE && str_eq(pname, "#size-cells"))
				n->size_cells = be32(value);
			else if (str_eq(pname, "compatible"))
				n->match = stringlist_has(value, len, compat);
			else if (str_eq(pname, "status"))
				n->enabled = stringlist_has(value, len, "okay") ||
					     stringlist_has(value, len, "ok");
			else if (str_eq(pname, "reg")) {
				n->reg = value;
				n->reg_len = len;
			} else if (str_eq(pname, "interrupts")) {
				n->irq = value;
				n->irq_len = len;
			}
			pos = align4(pos + 2 * CELL_SIZE + len);
			break;
		}
		case FDT_NOP:
			break;
		case FDT_END:
			return depth == 0 ? count : -1;
		default:
			return -1;
		}
	}
}
