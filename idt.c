/*
 * idt.c - Minimal IDT for catching exceptions during debug
 *
 * Sets up an IDT so that CPU exceptions print diagnostic info
 * via print() and halt, instead of triple-faulting into an instant reboot.
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
#include <printk.h>

#ifdef DEBUG

#define EXC_STUB_STRIDE	16
#define NUM_EXC		32

extern const char exc_stubs[];
extern const char exc_stubs_end[];

/*
 * Exception names packed into a single string to avoid pointer array
 * (which would land in .data due to relocations).
 */
static const char exc_names[] =
	"DE\0\0" "DB\0\0" "NMI\0" "BP\0\0" "OF\0\0" "BR\0\0" "UD\0\0" "NM\0\0"
	"DF\0\0" "??\0\0" "TS\0\0" "NP\0\0" "SS\0\0" "GP\0\0" "PF\0\0" "??\0\0"
	"MF\0\0" "AC\0\0" "MC\0\0" "XM\0\0" "VE\0\0" "CP\0\0";

#define EXC_NAME(vec) (&exc_names[(vec) * 4])
#define NUM_EXC_NAMES (sizeof(exc_names) / 4)

/* ---- Architecture-specific definitions ------------------------------- */

#ifdef __x86_64__

struct idt_entry {
	u16 offset_low;
	u16 selector;
	u8  ist;
	u8  type_attr;
	u16 offset_mid;
	u32 offset_high;
	u32 reserved;
} __packed;

static struct idt_entry idt[NUM_EXC];

struct exc_frame {
	u64 r15, r14, r13, r12, r11, r10, r9, r8;
	u64 rdi, rsi, rbp, rbx, rdx, rcx, rax;
	u64 vector, error_code;
	u64 rip, cs, rflags, rsp, ss;
};

__asmcall void idt_exc_handler(struct exc_frame *frame)
{
	u64 cr2;

	asm volatile("mov %%cr2, %0" : "=r"(cr2));

	print("\n*** Exception #");
	if (frame->vector < NUM_EXC_NAMES)
		print(EXC_NAME(frame->vector));
	else
		print("??");
	print("  load ");
	print_u32((u32)(uintptr_t)_start);
	print(" ***\n");
	print_u64(frame->error_code); print(" err  ");
	print_u64(frame->rip);        print(" rip\n");
	print_u64(cr2);               print(" cr2  ");
	print_u64(frame->rflags);     print(" rfl\n");
	print_u64(frame->rax);        print(" rax  ");
	print_u64(frame->rbx);        print(" rbx  ");
	print_u64(frame->rcx);        print(" rcx\n");
	print_u64(frame->rdx);        print(" rdx  ");
	print_u64(frame->rsi);        print(" rsi  ");
	print_u64(frame->rdi);        print(" rdi\n");
	print_u64(frame->rbp);        print(" rbp  ");
	print_u64(frame->rsp);        print(" rsp\n");

	udelay(2000000);
	print("stack:\n");
	hexdump((void *)(uintptr_t)frame->rsp, 128);

	udelay(10000000);
}

void setup_idt(void)
{
	struct {
		u16 limit;
		u64 base;
	} __packed idtr;
	unsigned int i;

	for (i = 0; i < NUM_EXC; i++) {
		uintptr_t handler = (uintptr_t)&exc_stubs[i * EXC_STUB_STRIDE];

		idt[i].offset_low  = handler & 0xFFFF;
		idt[i].selector    = CS_SEL64;
		idt[i].ist         = 0;
		idt[i].type_attr   = 0x8E; /* present, DPL=0, 64-bit interrupt gate */
		idt[i].offset_mid  = (handler >> 16) & 0xFFFF;
		idt[i].offset_high = (handler >> 32) & 0xFFFFFFFF;
		idt[i].reserved    = 0;
	}

	idtr.limit = sizeof(idt) - 1;
	idtr.base  = (uintptr_t)&idt[0];

	asm volatile("lidt %0" : : "m"(idtr));
}

/* ---- 32-bit IDT ------------------------------------------------------ */

#else

struct idt_entry {
	u16 offset_low;
	u16 selector;
	u8  zero;
	u8  type_attr;
	u16 offset_high;
} __packed;

static struct idt_entry idt[NUM_EXC];

struct exc_frame {
	u32 edi, esi, ebp, _esp, ebx, edx, ecx, eax;
	u32 vector, error_code;
	u32 eip, cs, eflags;
};

__asmcall void idt_exc_handler(struct exc_frame *frame)
{
	u32 cr2;
	void *stack = (void *)((uintptr_t)frame + sizeof(*frame));

	asm volatile("mov %%cr2, %0" : "=r"(cr2));

	print("\n*** Exception #");
	if (frame->vector < NUM_EXC_NAMES)
		print(EXC_NAME(frame->vector));
	else
		print("??");
	print("  load ");
	print_u32((u32)(uintptr_t)_start);
	print(" ***\n");
	print_u32(frame->error_code); print(" err  ");
	print_u32(frame->eip);        print(" eip  ");
	print_u32(cr2);               print(" cr2  ");
	print_u32(frame->eflags);     print(" efl\n");
	print_u32(frame->eax);        print(" eax  ");
	print_u32(frame->ebx);        print(" ebx  ");
	print_u32(frame->ecx);        print(" ecx  ");
	print_u32(frame->edx);        print(" edx\n");
	print_u32(frame->esi);        print(" esi  ");
	print_u32(frame->edi);        print(" edi  ");
	print_u32(frame->ebp);        print(" ebp  ");
	print_u32((u32)(uintptr_t)stack); print(" esp\n");

	udelay(2000000);
	print("stack:\n");
	hexdump(stack, 128);

	udelay(10000000);
}

void setup_idt(void)
{
	struct {
		u16 limit;
		u32 base;
	} __packed idtr;
	unsigned int i;

	for (i = 0; i < NUM_EXC; i++) {
		u32 handler = (u32)(uintptr_t)&exc_stubs[i * EXC_STUB_STRIDE];

		idt[i].offset_low  = handler & 0xFFFF;
		idt[i].selector    = CS_SEL32;
		idt[i].zero        = 0;
		idt[i].type_attr   = 0x8E; /* present, DPL=0, 32-bit interrupt gate */
		idt[i].offset_high = (handler >> 16) & 0xFFFF;
	}

	idtr.limit = sizeof(idt) - 1;
	idtr.base  = (u32)(uintptr_t)&idt[0];

	asm volatile("lidt %0" : : "m"(idtr));
}

#endif /* __x86_64__ */

#endif /* DEBUG */
