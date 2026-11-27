/*
 * Project: OS-001
 * Date: 2026-11-27
 * Authors: kwan, oda, sokpheng001
 *
 * Host tool: build the FAT32 test disk image. Written from Microsoft's "FAT32 File
 * System Specification" (FAT: General Overview of On-Disk Format, version 1.03, 2000;
 * section names below refer to it). Every file goes in the root directory, in
 * clusters allocated one after another. The file system leaves the last sectors of
 * the image free for the disk self-test, which writes there.
 *
 * Usage: mkfat32 IMAGE TOTAL_SECTORS TAIL_SECTORS FILE...
 * Each FILE is stored under its base name, which must be a valid upper-case 8.3 name.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECTOR_SIZE	 512u
#define SEC_PER_CLUS	 1u
#define RESERVED_SECTORS 32u
#define NUM_FATS	 2u
#define FSINFO_SECTOR	 1u
#define BACKUP_BOOT	 6u
#define ROOT_CLUSTER	 2u
#define MIN_FAT32_CLUSTERS                                                                         \
	65525u /* fewer clusters would make it FAT16 ("FAT Type Determination") */
#define DIR_ENTRY_SIZE 32u
#define MAX_FILES      (SECTOR_SIZE / DIR_ENTRY_SIZE - 1) /* one root cluster, minus the label */

#define FAT_MEDIA      0x0ffffff8u
#define FAT_EOC	       0x0fffffffu /* end of a cluster chain */
#define ATTR_ARCHIVE   0x20u
#define ATTR_VOLUME_ID 0x08u
#define MEDIA_FIXED    0xf8u

static uint8_t *image;
static uint32_t total_sectors, fs_sectors, fat_size;

static void put16(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v)
{
	put16(p, v);
	put16(p + 2, v >> 16);
}

static uint8_t *sector(uint32_t n)
{
	return image + (size_t)n * SECTOR_SIZE;
}

static uint32_t first_data_sector(void)
{
	return RESERVED_SECTORS + NUM_FATS * fat_size;
}

static void set_fat(uint32_t cluster, uint32_t value)
{
	for (uint32_t f = 0; f < NUM_FATS; f++)
		put32(sector(RESERVED_SECTORS + f * fat_size) + cluster * 4, value);
}

/* "HELLO.TXT" -> "HELLO   TXT" ("FAT Directory Structure", short names). */
static int short_name(const char *name, uint8_t out[11])
{
	const char *dot = strchr(name, '.');
	size_t base = dot ? (size_t)(dot - name) : strlen(name);
	size_t ext = dot ? strlen(dot + 1) : 0;

	if (base == 0 || base > 8 || ext > 3)
		return -1;
	memset(out, ' ', 11);
	for (size_t i = 0; name[i]; i++) {
		char c = name[i];

		if (c != '.' && !((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'))
			return -1;
	}
	memcpy(out, name, base);
	if (ext)
		memcpy(out + 8, dot + 1, ext);
	return 0;
}

static void boot_sector(uint8_t *b)
{
	/* BPB and FAT32 extended fields ("Boot Sector and BPB"). */
	b[0] = 0xeb;
	b[1] = 0x58;
	b[2] = 0x90;
	memcpy(b + 3, "OS-001  ", 8);
	put16(b + 11, SECTOR_SIZE);
	b[13] = SEC_PER_CLUS;
	put16(b + 14, RESERVED_SECTORS);
	b[16] = NUM_FATS;
	b[21] = MEDIA_FIXED;
	put16(b + 24, 32); /* sectors per track and heads: geometry is unused here */
	put16(b + 26, 64);
	put32(b + 32, fs_sectors);
	put32(b + 36, fat_size);
	put32(b + 44, ROOT_CLUSTER);
	put16(b + 48, FSINFO_SECTOR);
	put16(b + 50, BACKUP_BOOT);
	b[64] = 0x80;
	b[66] = 0x29;
	put32(b + 67, 0x05001001u); /* volume serial number */
	memcpy(b + 71, "OS-001 TEST", 11);
	memcpy(b + 82, "FAT32   ", 8);
	b[510] = 0x55;
	b[511] = 0xaa;
}

static void fsinfo_sector(uint8_t *s, uint32_t free_clusters, uint32_t next_free)
{
	/* "FAT32 FSInfo Sector Structure". */
	put32(s, 0x41615252u);
	put32(s + 484, 0x61417272u);
	put32(s + 488, free_clusters);
	put32(s + 492, next_free);
	put32(s + 508, 0xaa550000u);
}

int main(int argc, char **argv)
{
	if (argc < 4) {
		fprintf(stderr, "usage: %s IMAGE TOTAL_SECTORS TAIL_SECTORS FILE...\n", argv[0]);
		return 2;
	}
	total_sectors = (uint32_t)strtoul(argv[2], NULL, 0);
	fs_sectors = total_sectors - (uint32_t)strtoul(argv[3], NULL, 0);
	if (argc - 4 > (int)MAX_FILES) {
		fprintf(stderr, "mkfat32: at most %u files\n", MAX_FILES);
		return 1;
	}

	/* FAT size ("FAT Type Determination", the FATSz computation for FAT32). */
	uint32_t tmp1 = fs_sectors - RESERVED_SECTORS;
	uint32_t tmp2 = (256u * SEC_PER_CLUS + NUM_FATS) / 2u;

	fat_size = (tmp1 + tmp2 - 1) / tmp2;

	uint32_t clusters = (fs_sectors - first_data_sector()) / SEC_PER_CLUS;

	if (fs_sectors > total_sectors || clusters < MIN_FAT32_CLUSTERS) {
		fprintf(stderr, "mkfat32: %u sectors is too small for FAT32\n", fs_sectors);
		return 1;
	}
	image = calloc(total_sectors, SECTOR_SIZE);
	if (!image)
		return 1;

	boot_sector(sector(0));
	memcpy(sector(BACKUP_BOOT), sector(0), SECTOR_SIZE);
	set_fat(0, FAT_MEDIA);
	set_fat(1, FAT_EOC);
	set_fat(ROOT_CLUSTER, FAT_EOC);

	uint8_t *root = sector(first_data_sector());
	uint32_t next = ROOT_CLUSTER + 1;

	memcpy(root, "OS-001 TEST", 11);
	root[11] = ATTR_VOLUME_ID;
	for (int i = 4; i < argc; i++) {
		const char *path = argv[i];
		const char *base = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;
		uint8_t *ent = root + (uint32_t)(i - 3) * DIR_ENTRY_SIZE;
		FILE *f = fopen(path, "rb");

		if (!f || short_name(base, ent) != 0) {
			fprintf(stderr, "mkfat32: cannot add %s\n", path);
			return 1;
		}
		uint32_t size = 0, first = 0, prev = 0;
		size_t n;
		uint8_t buf[SECTOR_SIZE * SEC_PER_CLUS];

		while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
			if (next - 2 >= clusters) {
				fprintf(stderr, "mkfat32: disk full\n");
				return 1;
			}
			memcpy(sector(first_data_sector() + (next - 2) * SEC_PER_CLUS), buf, n);
			if (prev)
				set_fat(prev, next);
			else
				first = next;
			set_fat(next, FAT_EOC);
			prev = next++;
			size += (uint32_t)n;
		}
		fclose(f);
		ent[11] = ATTR_ARCHIVE;
		put16(ent + 20, first >> 16);
		put16(ent + 26, first & 0xffffu);
		put32(ent + 28, size);
	}

	uint32_t used = next - 2;

	fsinfo_sector(sector(FSINFO_SECTOR), clusters - used, next);
	memcpy(sector(BACKUP_BOOT + 1), sector(FSINFO_SECTOR), SECTOR_SIZE);

	/* Write only sectors holding data and seek over the zeros, so the image is sparse. */
	static const uint8_t zero[SECTOR_SIZE];
	FILE *out = fopen(argv[1], "wb");
	int ok = out != NULL;

	for (uint32_t n = 0; ok && n < total_sectors; n++) {
		if (memcmp(sector(n), zero, SECTOR_SIZE) == 0 && n + 1 < total_sectors)
			continue;
		ok = fseek(out, (long)n * SECTOR_SIZE, SEEK_SET) == 0 &&
		     fwrite(sector(n), SECTOR_SIZE, 1, out) == 1;
	}
	if (!ok || fclose(out) != 0) {
		fprintf(stderr, "mkfat32: cannot write %s\n", argv[1]);
		return 1;
	}
	return 0;
}
