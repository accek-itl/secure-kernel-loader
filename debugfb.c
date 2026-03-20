/*
 * debugfb.c - Framebuffer debug console
 *
 * In 32-bit mode: enables PAE paging to map a GPU framebuffer (above 4GB)
 * into the 32-bit virtual address space.
 * In 64-bit mode: adds page table entries to the existing identity map
 * set up by head.S for the >4GB framebuffer region.
 *
 * Renders text with an 8x8 bitmap font stretched to 8x16.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <defs.h>
#include <types.h>
#include <boot.h>

#ifdef DEBUGFB_BASE

extern const u8 font_8x8[95][8];

#ifndef DEBUGFB_STRIDE
#define DEBUGFB_STRIDE	(DEBUGFB_W * 4)
#endif

/* Character cell dimensions: 8x8 font stretched to 8x16 */
#define CHAR_W		8
#define CHAR_H		16
#define FONT_H		8

#define COLS		(DEBUGFB_W / CHAR_W)
#define ROWS		(DEBUGFB_H / CHAR_H)

static int cur_col;
static int cur_row;

static volatile u32 *fb_virt;

/* ---- Page table setup (architecture-specific) ------------------------ */

#ifdef __x86_64__

/*
 * In 64-bit mode, head.S has already set up paging with an identity map of
 * 0–4GB.  For a framebuffer above 4GB we add entries to the existing page
 * tables.  We need one extra L3 (PDPT) and one extra L2 (PD) page.
 */
extern u64 l4_identmap[];
extern u64 l3_identmap[];

static u64 fb_l3[512] __page_data;
static u64 fb_l2[512] __page_data;

static void debugfb_map(void)
{
	unsigned int l4_idx = (DEBUGFB_BASE >> 39) & 0x1FF;
	unsigned int l3_idx = (DEBUGFB_BASE >> 30) & 0x1FF;
	unsigned int l2_start = (DEBUGFB_BASE >> 21) & 0x1FF;
	unsigned int n_pages, i;

	n_pages = ((u64)(DEBUGFB_STRIDE) * (u64)(DEBUGFB_H)
		   + (DEBUGFB_BASE & 0x1FFFFFULL) + 0x1FFFFF) >> 21;

	for (i = 0; i < n_pages; i++) {
		u64 fb_phys = (DEBUGFB_BASE & ~0x1FFFFFULL) + (u64)i * 0x200000ULL;

		fb_l2[l2_start + i] =
			fb_phys | _PAGE_PCD | _PAGE_PSE | _PAGE_AD | _PAGE_RW | _PAGE_PRESENT;
	}

	if (l4_idx == 0) {
		/* FB is in first 512GB — use existing L3 */
		l3_identmap[l3_idx] =
			(uintptr_t)fb_l2 | _PAGE_AD | _PAGE_RW | _PAGE_PRESENT;
	} else {
		/* FB is above 512GB — need new L3 page */
		fb_l3[l3_idx] =
			(uintptr_t)fb_l2 | _PAGE_AD | _PAGE_RW | _PAGE_PRESENT;
		l4_identmap[l4_idx] =
			(uintptr_t)fb_l3 | _PAGE_AD | _PAGE_RW | _PAGE_PRESENT;
	}

	/* Flush TLB */
	asm volatile("mov %%cr3, %%rax; mov %%rax, %%cr3" ::: "rax", "memory");

	/* Identity-mapped: virtual = physical */
	fb_virt = (volatile u32 *)(uintptr_t)DEBUGFB_BASE;
}

void debugfb_teardown(void)
{
	/* No-op: head.S handles full paging teardown */
	fb_virt = NULL;
}

#else /* 32-bit PAE */

/*
 * PAE PDPT: 4 entries, 8 bytes each = 32 bytes, must be 32-byte aligned.
 * PAE PDs:  4 pages of 512 entries (8 bytes each) = 16KB total, page-aligned.
 * PAE PT:   1 page of 512 entries for the first 2MB (4KB granularity),
 *           avoiding undefined behaviour from a 2MB page spanning mixed MTRRs.
 */
static u64 pdpt[4] __aligned(32);
static u64 pd[4][512] __page_data;
static u64 pt0[512] __page_data;

/* Virtual base where the framebuffer is mapped */
#define FB_VIRT_BASE	0x80000000UL

static void debugfb_map(void)
{
	unsigned int i, j;
	u64 phys;
	unsigned int pd_idx, pde_start, n_pages;

	/* Identity-map 0–4GB with 2MB pages */
	for (i = 0; i < 4; i++) {
		for (j = 0; j < 512; j++) {
			phys = (u64)i * 0x40000000ULL + (u64)j * 0x200000ULL;
			pd[i][j] = phys | _PAGE_PSE | _PAGE_AD | _PAGE_RW | _PAGE_PRESENT;
		}
		pdpt[i] = (uintptr_t)&pd[i][0] | _PAGE_PRESENT;
	}

	/*
	 * Use 4KB pages for the first 2MB to avoid undefined behaviour
	 * from a 2MB superpage spanning mixed MTRR types (e.g. WB RAM
	 * and UC VGA hole at 0xA0000).
	 */
	for (i = 0; i < 512; i++)
		pt0[i] = (u64)i * PAGE_SIZE | _PAGE_AD | _PAGE_RW | _PAGE_PRESENT;
	pd[0][0] = (uintptr_t)&pt0[0] | _PAGE_AD | _PAGE_RW | _PAGE_PRESENT;

	/*
	 * Override PD entries for the framebuffer virtual window at FB_VIRT_BASE.
	 * FB_VIRT_BASE = 0x80000000 falls in PD index 2, entry 0+.
	 */
	pd_idx = FB_VIRT_BASE / 0x40000000UL;
	pde_start = (FB_VIRT_BASE % 0x40000000UL) / 0x200000UL;

	n_pages = ((u32)(DEBUGFB_STRIDE) * (u32)(DEBUGFB_H)
		   + (u32)(DEBUGFB_BASE & 0x1FFFFFULL)
		   + 0x1FFFFF) / 0x200000;

	for (i = 0; i < n_pages; i++) {
		u64 fb_phys = (DEBUGFB_BASE & ~0x1FFFFFULL) + (u64)i * 0x200000ULL;

		pd[pd_idx][pde_start + i] =
			fb_phys | _PAGE_PCD | _PAGE_PSE | _PAGE_AD | _PAGE_RW | _PAGE_PRESENT;
	}

	/* Enable PAE paging */
	asm volatile(
		"mov %%cr4, %%eax\n\t"
		"or  %[pae], %%eax\n\t"
		"mov %%eax, %%cr4\n\t"
		"mov %[cr3], %%cr3\n\t"
		"mov %%cr0, %%eax\n\t"
		"or  %[pg], %%eax\n\t"
		"mov %%eax, %%cr0\n\t"
		:
		: [pae] "i" (CR4_PAE),
		  [cr3] "r" ((u32)(uintptr_t)pdpt),
		  [pg]  "i" (CR0_PG)
		: "eax", "memory"
	);

	/* Compute virtual FB pointer (account for sub-2MB offset) */
	fb_virt = (volatile u32 *)(FB_VIRT_BASE + (u32)(DEBUGFB_BASE & 0x1FFFFFULL));
}

void debugfb_teardown(void)
{
	fb_virt = NULL;

	/* Disable paging (safe: identity-mapped) */
	asm volatile(
		"mov %%cr0, %%eax\n\t"
		"and %[mask], %%eax\n\t"
		"mov %%eax, %%cr0\n\t"
		"mov %%cr4, %%eax\n\t"
		"and %[pae_mask], %%eax\n\t"
		"mov %%eax, %%cr4\n\t"
		:
		: [mask]     "i" (~CR0_PG),
		  [pae_mask] "i" (~CR4_PAE)
		: "eax", "memory"
	);
}

#endif /* __x86_64__ */

/* ---- Common init / character rendering ------------------------------- */

void debugfb_init(void)
{
	unsigned int i;

	debugfb_map();

	/* Fill framebuffer with grayscale noise as a visual "I'm alive" signal */
	{
		u32 stride_px = (u32)DEBUGFB_STRIDE / 4;
		u32 rng = 0xdeadbeef;

		for (i = 0; i < stride_px * (u32)DEBUGFB_H; i++) {
			rng ^= rng << 13;
			rng ^= rng >> 17;
			rng ^= rng << 5;
			u8 g = (rng & 7) * 2;
			fb_virt[i] = (u32)g << 16 | (u32)g << 8 | g;
		}
	}

	cur_col = 0;
	cur_row = 0;
}

static void clear_line(int row)
{
	u32 stride_px = (u32)DEBUGFB_STRIDE / 4;
	volatile u32 *line = fb_virt + (u32)row * CHAR_H * stride_px;
	unsigned int i;

	for (i = 0; i < stride_px * CHAR_H; i++)
		line[i] = 0;
}

static void render_char(char c, int col, int row)
{
	const u8 *glyph;
	u32 stride_px = (u32)DEBUGFB_STRIDE / 4;
	volatile u32 *base;
	int fy, fx;

	if (c < 0x20 || c > 0x7e)
		c = '?';

	glyph = font_8x8[c - 0x20];
	base = fb_virt + (u32)row * CHAR_H * stride_px + (u32)col * CHAR_W;

	for (fy = 0; fy < FONT_H; fy++) {
		u8 bits = glyph[fy];
		volatile u32 *row0 = base + (fy * 2) * stride_px;
		volatile u32 *row1 = row0 + stride_px;

		for (fx = 0; fx < CHAR_W; fx++) {
			u32 color = (bits & (0x80 >> fx)) ? 0x00FFFFFF : 0x00000000;
			row0[fx] = color;
			row1[fx] = color;
		}
	}
}

void debugfb_putchar(char c)
{
	if (!fb_virt)
		return;

	if (c == '\n' || c == '\r') {
		cur_col = 0;
		if (c == '\n') {
			udelay(100000);
			cur_row++;
			if (cur_row >= ROWS) {
				cur_row = 0;
				clear_line(cur_row);
			}
		}
		return;
	}

	render_char(c, cur_col, cur_row);
	cur_col++;

	if (cur_col >= COLS) {
		cur_col = 0;
		cur_row++;
		if (cur_row >= ROWS) {
			cur_row = 0;
		}
		clear_line(cur_row);
	}
}

#endif /* DEBUGFB_BASE */
