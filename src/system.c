// This file is used to override the reset handler

#include <libopencm3/cm3/vector.h>
#include <libopencm3/cm3/scb.h>
#include <sys/types.h>

/* Symbols exported by the linker script(s): */
extern unsigned _data_loadaddr, _data, _edata, _ebss, _stack;
typedef void (*funcp_t) (void);
extern funcp_t __preinit_array_start, __preinit_array_end;
extern funcp_t __init_array_start, __init_array_end;
extern funcp_t __fini_array_start, __fini_array_end;

void main(void);
caddr_t _sbrk(int incr);

// from libopencm3: lib/stm32/f4/vector_chipset.c
static void pre_main(void)
{
  /* Enable access to Floating-Point coprocessor. */
  SCB_CPACR |= SCB_CPACR_FULL * (SCB_CPACR_CP10 | SCB_CPACR_CP11);
}


void __attribute__ ((naked)) reset_handler(void)
{
#ifndef DEBUG
	volatile unsigned *src;
#endif

	volatile unsigned *dest;

	funcp_t *fp;

#ifndef DEBUG
	for (src = &_data_loadaddr, dest = &_data;
		dest < &_edata;
		src++, dest++) {
		*dest = *src;
	}
#else
  // set vector table offset register to the RAM base address
  SCB_VTOR = 0x20000000;
  dest = &_edata;
#endif

	while (dest < &_ebss) {
		*dest++ = 0;
	}

	/* Constructors. */
	for (fp = &__preinit_array_start; fp < &__preinit_array_end; fp++) {
		(*fp)();
	}
	for (fp = &__init_array_start; fp < &__init_array_end; fp++) {
		(*fp)();
	}

	/* might be provided by platform specific vector.c */
	pre_main();

	/* Call the application's entry point. */
	main();

	/* Destructors. */
	for (fp = &__fini_array_start; fp < &__fini_array_end; fp++) {
		(*fp)();
	}

}

caddr_t _sbrk(int incr)
{
  extern char end;    /* Defined by the linker */
  static char *heap_end;
  char *prev_heap_end;

  if (heap_end == 0) {
    heap_end = &end;
  }
  prev_heap_end = heap_end;
  // if (heap_end + incr > stack_ptr) {
  //   // heap and stack collision -> hang
  //   while(1);
  // }

  heap_end += incr;
  return (caddr_t) prev_heap_end;
}
