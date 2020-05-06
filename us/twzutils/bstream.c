#include <twz/bstream.h>
#include <twz/gate.h>

TWZ_GATE(bstream_read, BSTREAM_GATE_READ);
TWZ_GATE(bstream_write, BSTREAM_GATE_WRITE);
TWZ_GATE(bstream_poll, BSTREAM_GATE_POLL);

int main()
{
	__builtin_unreachable();
}
