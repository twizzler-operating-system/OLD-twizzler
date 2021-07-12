#pragma once

/** @file
 * @brief Secure random number generation facilities.
 *
 * This system uses entropy sources registered during kernel startup to source seed material for a
 * CSPRNG. All random numbers are generated from the CSPRNG, and never use the entropy sources
 * directly. \ref rand_getbytes is probably the function you want if you want random bytes.
 */

#include <lib/list.h>

/** A source of entropy that the CSPRNG can use for seeding. */
struct entropy_source {
	/** Human-readable name */
	const char *name;
	/** Number of times this has been used */
	size_t uses;
	/** Write at most len bytes of random data into data. Returns the number of bytes actually
	 * written. */
	ssize_t (*get)(void *data, size_t len);
	struct list entry;
};

/** Get random bytes, reseeding from entropy sources if necessary. Cryptographically secure. Sources
 * from a CSPRNG seeded by entropy sources.
 * @param data buffer to write random bytes into.
 * @param length length of the buffer. Will either fill the buffer or not.
 * @param flags. Currently unused, must be zero.
 * @return 0 on success -errno on failure.
 */
int rand_getbytes(void *data, size_t length, int flags);

/** Register a new entropy source to the system. */
void rand_register_entropy_source(struct entropy_source *src);

/** Get len random bytes, copied into data, from the CSPRNG, without reseeding. */
void rand_csprng_get(void *data, size_t len);

/** Reseed the CSPRNG using len bytes stored in entropy */
void rand_csprng_reseed(void *entropy, size_t len);

#define RANDSIZL (8)
#define RANDSIZ (1 << RANDSIZL)
