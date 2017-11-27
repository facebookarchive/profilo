#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#ifdef __linux__
#include <malloc.h>
#endif
#include <setjmp.h>
#include <errno.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/time.h>
#include "sigmux.h"

static void
handle_alarm()
{
  fprintf(stderr, "got alarm (base)\n");
}

static sigjmp_buf jmp;

static enum sigmux_action
handle_testreg(struct sigmux_siginfo* siginfo,
               void* handler_data)
{
  const char* name = handler_data;
  fprintf(stderr, "got signal (testreg) name=%s\n", name);
  if (!strcmp(handler_data, "reg1")) {
    sigmux_longjmp(siginfo, jmp, 1);
  }

  return SIGMUX_CONTINUE_SEARCH;
}

static struct sigmux_registration*
register_testreg(const char* name)
{
  struct sigmux_registration* reg;
  sigset_t signals;

  sigemptyset(&signals);
  sigaddset(&signals, SIGALRM);
  reg = sigmux_register(&signals, handle_testreg, (void*) name, 0);
  if (reg == NULL) {
    fprintf(stderr, "sigmux_register failed: %s\n", strerror(errno));
    abort();
  }

  fprintf(stderr, "registered handler %p for %s\n", reg, name);
  return reg;
}

int
main()
{
  struct sigmux_registration* volatile r1 = NULL;

  signal(SIGALRM, handle_alarm);

  if (sigmux_init(SIGALRM) != 0) {
    fprintf(stderr, "sigmux_init failed: %s\n", strerror(errno));
    return 1;
  }

  if (sigsetjmp(jmp, 1) == 1) {
    fprintf(stderr, "got longjmp from %p\n", r1);
    sigmux_unregister(r1);
    goto loop;
  }

  r1 = register_testreg("reg1");
  register_testreg("reg2");

  loop:
  for (;;) {
    /* struct itimerval tv = { */
    /*   .it_value.tv_usec = 1000, */
    /* }; */
    /* setitimer(ITIMER_REAL, &tv, NULL); */
    alarm(1);
    pause();
  }

  fprintf(stderr, "all done\n");
}
