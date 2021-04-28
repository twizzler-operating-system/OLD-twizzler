#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <twz/keyring.h>
#include <twz/meta.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/ptr.h>
#include <twz/sys/kso.h>
#include <twz/sys/sys.h>
#include <twz/sys/thread.h>
#include <twz/user.h>
#include <unistd.h>

#include <twz/sec/security.h>

extern char **environ;
void tmain(const char *username)
{
	char userpath[1024];
	snprintf(userpath, 512, "%s.user", username);

	objid_t uid;
	int r;
	r = twz_name_resolve(NULL, userpath, NULL, 0, &uid);
	if(r) {
		fprintf(stderr, "failed to resolve '%s'\n", userpath);
		exit(1);
	}

	twzobj user;
	twz_object_init_guid(&user, uid, FE_READ);
	struct user_hdr *uh = twz_object_base(&user);

	char userstring[128];
	snprintf(userstring, 128, IDFMT, IDPR(uid));
	setenv("TWZUSER", userstring, 1);
	setenv("USER", username, 1);
	char *ps1 = NULL;
	asprintf(&ps1, "\\e[36m\\u\\e[m@\\e[35m\\h\\e[m:\\e[32m\\w\\e[m $ ");
	setenv("PS1", ps1, 1);

	r = sys_detach(0, 0, TWZ_DETACH_ONENTRY | TWZ_DETACH_ONSYSCALL(SYS_BECOME), KSO_SECCTX);
	if(r) {
		r = sys_detach(0, 0, TWZ_DETACH_ONENTRY | TWZ_DETACH_ONSYSCALL(SYS_BECOME), KSO_SECCTX);
		if(r) {
			fprintf(stderr, "failed to detach from login context\n");
		}
		// exit(1);
	}
	/* TODO: dont do this */
	twzobj context;
	if(twz_object_new(
	     &context, NULL, NULL, OBJ_VOLATILE, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_DFL_USE)) {
		abort();
	}
	twz_sctx_init(&context, "__generic");
	if(sys_attach(0, twz_object_guid(&context), 0, 2)) {
		abort();
	}

	if(uh->dfl_secctx) {
		r = sys_attach(0, uh->dfl_secctx, 0, KSO_SECCTX);
		if(r) {
			fprintf(stderr, "failed to attach to user context\n");
			exit(1);
		}
		char context[128];
		sprintf(context, IDFMT, IDPR(uh->dfl_secctx));
		setenv("TWZUSERSCTX", context, 1);
	}

	if(uh->kr && 0) {
		struct keyring_hdr *krh = twz_object_lea(&user, uh->kr);

		if(krh->dfl_pubkey) {
			twzobj keyring;
			twz_object_init_ptr(&keyring, krh);
			struct key_hdr *key = twz_object_lea(&keyring, krh->dfl_pubkey);
			twzobj keyobj;
			twz_object_init_ptr(&keyobj, key);
			char keystr[128];
			sprintf(keystr, IDFMT, IDPR(twz_object_guid(&keyobj)));
			setenv("TWZUSERKU", keystr, 1);

			struct key_hdr *pkey = twz_object_lea(&keyring, krh->dfl_prikey);
			twzobj pkeyobj;
			twz_object_init_ptr(&pkeyobj, pkey);
			char pkeystr[128];
			sprintf(pkeystr, IDFMT, IDPR(twz_object_guid(&pkeyobj)));
			setenv("TWZUSERKEY", pkeystr, 1);

			char keyringstr[128];
			sprintf(keyringstr, IDFMT, IDPR(twz_object_guid(&keyring)));
			setenv("TWZUSERKRING", keyringstr, 1);
		} else {
			fprintf(stderr, "warning - no default key for user\n");
		}
	} else {
		fprintf(stderr, "warning - no keyring for user\n");
	}

	kso_set_name(NULL, "[instance] shell [user %s]", username);
	// execv("/usr/bin/ycsbc",
	// (char *[]){ "ycsbc", "-db", "sqlite", "-P", "/usr/share/ycsbc/workloadf.spec", NULL });
	r = execv("/usr/bin/bash", (char *[]){ "/usr/bin/bash", NULL });
	fprintf(stderr, "failed to exec shell: %d", r);
	exit(1);
}

#include <twz/debug.h>
int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	// try_test();
	// int k = 0x7fffffff;
	//	k += argc;
	printf("Setting SCE to AUX.\n\n");
	//	bar();

	for(;;) {
		char buffer[1024];
		printf("Twizzler Login: ");
		fflush(NULL);
		fgets(buffer, 1024, stdin);
		// strcpy(buffer, "bob");

		// debug_printf("LOGIN HIIIII %s\n", buffer);
		// fprintf(stderr, "ADAWDAWDAWDAWEAWDAWDAWD\n");
		// continue;
		char *n = strchr(buffer, '\n');
		if(n)
			*n = 0;
		if(n == buffer) {
			printf("AUTO LOGIN: bob\n");
			strcpy(buffer, "bob");
		}

		pid_t pid;
		if(!(pid = fork())) {
			tmain(buffer);
		}
		if(pid == -1) {
			warn("fork");
			continue;
		}

		int status;
		pid_t wp;
		while((wp = wait(&status)) != pid) {
			if(wp < 0) {
				if(errno == EINTR) {
					continue;
				}
				warn("wait");
				break;
			}
		}
		for(volatile long i = 0; i < 100000000; i++) {
		}
	}
}
