/*
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

#include <boot.h>
#include <debugfb.h>
#include <string.h>
#include <types.h>

#ifdef DEBUG

#ifdef DEBUGMEM_BASE

static bool debugmem_initialized;
static char* debugmem_ptr;

static void print_char(char c)
{
    if (!debugmem_initialized) {
        debugmem_ptr = (char*)DEBUGMEM_BASE;
        memset(debugmem_ptr, 0xaa, 0x1000);
        debugmem_initialized = true;
    }

    *debugmem_ptr = c;
    __asm__ __volatile__("clflush (%0)" :: "r"(debugmem_ptr) : "memory");
    debugmem_ptr++;
    *debugmem_ptr = '\0';
    __asm__ __volatile__("clflush (%0)" :: "r"(debugmem_ptr) : "memory");
}

#elif defined(DEBUGFB_BASE)

static void print_char(char c)
{
    debugfb_putchar(c);
}

#else

static void print_char(char c)
{
    while ( !(inb(0x3f8 + 5) & 0x20) )
        ;

    outb(c, 0x3f8);
}

#endif

void print(const char * txt)
{
    while ( *txt != '\0' )
    {
        if ( *txt == '\n' )
            print_char('\r');
        print_char(*txt++);
    }
}

static void print_hex_nibble(u8 nibble)
{
    nibble &= 0xf;
    print_char(nibble < 10 ? '0' + nibble : 'a' + nibble - 10);
}

static void print_hex(u64 val, int size)
{
    int i;

    print_char('0');
    print_char('x');
    for (i = (size - 1) * 8; i >= 0; i -= 4) {
        u8 nibble = (val >> i) & 0xf;
        print_char(nibble < 10 ? '0' + nibble : 'a' + nibble - 10);
    }
}

void print_p(const void * _p)
{
    print_hex((u64)(uintptr_t)_p, sizeof(_p));
    print_char(':');
    print_char(' ');
}

void print_u32(u32 v)
{
    print_hex(v, 4);
}

void print_u64(u64 v)
{
    print_hex(v, 8);
}

static void print_b(char p)
{
    print_hex_nibble(p >> 4);
    print_hex_nibble(p);
    print_char(' ');
}

static inline int isprint(int c)
{
    return c >= ' ' && c <= '~';
}

void hexdump(const void *memory, size_t length)
{
    int i;
    u8 *line;
    int all_zero = 0;
    int all_one = 0;
    size_t num_bytes;

    for ( i = 0; i < length; i += 16 )
    {
        int j;
        num_bytes = 16;
        line = ((u8 *)memory) + i;

        all_zero++;
        all_one++;
        for ( j = 0; j < num_bytes; j++ )
        {
            if ( line[j] != 0 )
            {
                all_zero = 0;
                break;
            }
        }

        for ( j = 0; j < num_bytes; j++ )
        {
            if ( line[j] != 0xff )
            {
                all_one = 0;
                break;
            }
        }

        if ( (all_zero < 2) && (all_one < 2) )
        {
            print_p(memory + i);
            for ( j = 0; j < num_bytes; j++ )
                print_b(line[j]);
            for ( ; j < 16; j++ )
                print("   ");
            print("  ");
            for ( j = 0; j < num_bytes; j++ )
                isprint(line[j]) ? print_char(line[j]) : print_char('.');
            print("\n");
        }
        else if ( (all_zero == 2) || (all_one == 2) )
        {
            print("...\n");
        }
    }
}

#endif /* DEBUG */
