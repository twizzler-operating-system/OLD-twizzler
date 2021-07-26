#define PTY_BUFFER_SZ 1024

struct pty_hdr {
	struct bstream_hdr *stoc;
	struct bstream_hdr *ctos;
	struct twzio_hdr io;
	struct termios termios;
	struct winsize wsz;
	struct mutex buffer_lock;
	size_t bufpos;
	char buffer[PTY_BUFFER_SZ];
};

struct pty_client_hdr {
	struct pty_hdr *server;
	struct twzio_hdr io;
};


