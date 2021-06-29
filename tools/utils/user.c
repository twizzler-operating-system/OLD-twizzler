/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <err.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <twz/obj.h>
#include <twz/obj/keyring.h>
#include <twz/obj/user.h>
#include <twz/sys/obj.h>

#include "common.h"

void usage(void)
{
	fprintf(stderr, "mkuser -u userfile -r ringfile username\n");
	exit(1);
}

int main(int argc, char **argv)
{
	int c;
	char *userfile = NULL, *ringfile = NULL, *sctx = NULL;
	while((c = getopt(argc, argv, "u:r:s:")) != EOF) {
		switch(c) {
			case 'u':
				userfile = optarg;
				break;
			case 'r':
				ringfile = optarg;
				break;
			case 's':
				sctx = optarg;
				break;
			default:
				usage();
		}
	}

	char *username = argv[optind];

	if(!userfile || !ringfile || !username)
		usage();

	FILE *u = fopen(userfile, "w");
	if(!u)
		err(1, "fopen: %s", userfile);
	FILE *r = fopen(ringfile, "w");
	if(!r)
		err(1, "fopen: %s", ringfile);

	struct keyring_hdr kh = {
		.dfl_prikey = twz_ptr_rebase(1, (void *)(OBJ_NULLPAGE_SIZE)),
		.dfl_pubkey = twz_ptr_rebase(2, (void *)(OBJ_NULLPAGE_SIZE)),
	};

	objid_t sc = sctx ? str_to_objid(sctx) : 0;
	struct user_hdr uh = {
		.kr = twz_ptr_rebase(1, (void *)(OBJ_NULLPAGE_SIZE)),
		.name = (void *)(OBJ_NULLPAGE_SIZE + sizeof(struct user_hdr)),
		.flags = 0,
		.dfl_secctx = sc,
	};

	fwrite(&kh, sizeof(kh), 1, r);
	fwrite(&uh, sizeof(uh), 1, u);
	fwrite(username, 1, strlen(username) + 1, u);

	fclose(u);
	fclose(r);
	return 0;
}
