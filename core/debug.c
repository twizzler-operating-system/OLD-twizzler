#include <trace.h>
#include <interrupt.h>
#include <thread.h>
#include <processor.h>

int trace_indent_level = 0;

/* TODO: clean up */
int serial_received();
int serial_getc();
void kernel_debug_entry(void)
{
	//while(!serial_received());

	printk("::PREPANIC %d %d\n", serial_received(), serial_getc());
	arch_processor_reset();
}

