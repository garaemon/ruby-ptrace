/*
 * ptrace.c: Simple ptrace wrapper for Ruby
 *
 * written by Koichi Sasada
 * Wed Sep 10 03:48:10 2008
 */
#include <ruby.h>

#include <sys/ptrace.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <mach/thread_status.h>
#if defined(__APPLE__)
typedef int ptrace_request;
#else
typedef enum __ptrace_request ptrace_request;
#endif
//#define HAVE_PTRACE_SYSEMU 1

#define CALL_PTRACE(ret, request, pid, addr, data) do { \
	ret = ptrace(request, pid, addr, data); \
	if (ret == -1) { \
	    ptrace_error(request, errno); \
	} \
    } while (0)

#define UNSUPPORTED() \
  rb_raise(rb_eRuntimeError, "unsupported method %s:%d", __FILE__, __LINE__)

static VALUE rb_ePTraceError;
static VALUE rb_sPTraceRegStruct;
static VALUE rb_sPTraceFPRegStruct;
static ID id_ptrace_pid;

static pid_t
get_pid(VALUE self)
{
    VALUE pidv = rb_ivar_get(self, id_ptrace_pid);
    pid_t pid = (pid_t)NUM2LONG(pidv);

    if (!RTEST(pidv) || pid == 0) {
	rb_raise(rb_ePTraceError, "null pid");
    }

    return pid;
}

static void
ptrace_error(ptrace_request request, int err)
{
    rb_raise(rb_ePTraceError, "ptrace error (%d): %s (%d)",
	     request, strerror(err), err);
}

static VALUE
ptrace_peek(VALUE self, ptrace_request request, VALUE addr)
{
    pid_t pid = get_pid(self);
    void *ptr = (void *)NUM2ULONG(addr);
    long ret;

    ret = ptrace(request, pid, ptr, 0);
    if (ret == -1) {
	if (errno != 0) {
	    ptrace_error(request, errno);
	}
    }

    return LONG2NUM(ret);
}

static VALUE
ptrace_peektext(VALUE self, VALUE addr)
{
    return ptrace_peek(self, PT_READ_I, addr);
}

static VALUE
ptrace_peekdata(VALUE self, VALUE addr)
{
    return ptrace_peek(self, PT_READ_D, addr);
}

static VALUE
ptrace_peekuser(VALUE self, VALUE addr)
{
     return ptrace_peek(self, PT_READ_U, addr);
}

static VALUE
ptrace_poke(VALUE self, ptrace_request request, VALUE addr, VALUE data)
{
    pid_t pid = get_pid(self);
    void *addr_ptr = (void *)NUM2ULONG(addr);
    void *long_data = (void *)NUM2LONG(data);
    long ret;

    CALL_PTRACE(ret, request, pid, addr_ptr, long_data);

    return Qnil;
}

static VALUE
ptrace_poketext(VALUE self, VALUE addr, VALUE data)
{
    return ptrace_poke(self, PT_WRITE_I, addr, data);
}

static VALUE
ptrace_pokedata(VALUE self, VALUE addr, VALUE data)
{
    return ptrace_poke(self, PT_WRITE_D, addr, data);
}

static VALUE
ptrace_pokeuser(VALUE self, VALUE addr, VALUE data)
{
    return ptrace_poke(self, PT_WRITE_U, addr, data);
}

#if defined(__APPLE__)
static VALUE
ptrace_getregs(VALUE self)
{
    x86_thread_state_t thread_state;
    VALUE ret = Qnil;
    int state_count = x86_THREAD_STATE_COUNT; /* only x86 supported */
    kern_return_t thread_status = thread_get_status(pthread_self(),
                                                    x86_THREAD_STATE,
                                                    &thread_state,
                                                    &state_count);
    if ( thread_status != 0 )
    {
        rb_raise(rb_ePTraceError, "thread_get_state failed with error code: %d",
                 thread_status);
    }

    ret = rb_struct_new(rb_sPTraceRegStruct,
                        ULONG2NUM(thread_state.uts.ts64.__rax), 
                        ULONG2NUM(thread_state.uts.ts64.__rbx),
                        ULONG2NUM(thread_state.uts.ts64.__rcx),
                        ULONG2NUM(thread_state.uts.ts64.__rdi),
                        ULONG2NUM(thread_state.uts.ts64.__rsi),
                        ULONG2NUM(thread_state.uts.ts64.__rbp),
                        ULONG2NUM(thread_state.uts.ts64.__rsp),
                        ULONG2NUM(thread_state.uts.ts64.__r8), 
                        ULONG2NUM(thread_state.uts.ts64.__r9),
                        ULONG2NUM(thread_state.uts.ts64.__r10),
                        ULONG2NUM(thread_state.uts.ts64.__r11),
                        ULONG2NUM(thread_state.uts.ts64.__r12),
                        ULONG2NUM(thread_state.uts.ts64.__r13),
                        ULONG2NUM(thread_state.uts.ts64.__r14),
                        ULONG2NUM(thread_state.uts.ts64.__r15),
                        ULONG2NUM(thread_state.uts.ts64.__rip),
                        ULONG2NUM(thread_state.uts.ts64.__rflags),
                        ULONG2NUM(thread_state.uts.ts64.__cs),
                        ULONG2NUM(thread_state.uts.ts64.__fs),
                        ULONG2NUM(thread_state.uts.ts64.__gs));
                        
    return ret;
}
#else
#ifdef PT_GETREGS
static VALUE
ptrace_getregs(VALUE self)
{
    struct user_regs_struct urs;
    void *data_ptr = (void *)&urs;
    pid_t pid = get_pid(self);
    long ret;
    VALUE v = Qnil;

    CALL_PTRACE(ret, PT_GETREGS, pid, 0, data_ptr);

    v = rb_struct_new(rb_sPTraceRegStruct,
		      ULONG2NUM(urs.ebx), ULONG2NUM(urs.ecx), ULONG2NUM(urs.edx),
		      ULONG2NUM(urs.esi), ULONG2NUM(urs.edi), ULONG2NUM(urs.ebp),
		      ULONG2NUM(urs.eax), ULONG2NUM(urs.xds), ULONG2NUM(urs.xes),
		      ULONG2NUM(urs.xfs), ULONG2NUM(urs.xgs), ULONG2NUM(urs.orig_eax),
		      ULONG2NUM(urs.eip), ULONG2NUM(urs.xcs), ULONG2NUM(urs.eflags),
		      ULONG2NUM(urs.esp), ULONG2NUM(urs.xss));
    return v;
}
#else
static VALUE
ptrace_getregs(VALUE self)
{
    UNSUPPORTED();
    return Qnil;
}
#endif  /* PT_GETREGS */
#endif  /* __APPLE__ */

static VALUE
ptrace_getfpregs(VALUE self)
{
    UNSUPPORTED();
    return Qnil;
}

static int
signo_symbol_to_int(VALUE sym)
{
#define SI_SIGNO(v) if (ID2SYM(rb_intern(#v)) == sym) return v;
    SI_SIGNO(SIGHUP);
    SI_SIGNO(SIGINT);
    SI_SIGNO(SIGQUIT);
    SI_SIGNO(SIGILL);
    SI_SIGNO(SIGTRAP);
    SI_SIGNO(SIGABRT);
    SI_SIGNO(SIGIOT);
    SI_SIGNO(SIGBUS);
    SI_SIGNO(SIGFPE);
    SI_SIGNO(SIGKILL);
    SI_SIGNO(SIGUSR1);
    SI_SIGNO(SIGSEGV);
    SI_SIGNO(SIGUSR2);
    SI_SIGNO(SIGPIPE);
    SI_SIGNO(SIGALRM);
    SI_SIGNO(SIGTERM);
#ifdef SIGSTKFLT
    SI_SIGNO(SIGSTKFLT);
#endif
    SI_SIGNO(SIGCHLD);
#ifdef SIGCLD
    SI_SIGNO(SIGCLD);
#endif
    SI_SIGNO(SIGCONT);
    SI_SIGNO(SIGSTOP);
    SI_SIGNO(SIGTSTP);
    SI_SIGNO(SIGTTIN);
    SI_SIGNO(SIGTTOU);
    SI_SIGNO(SIGURG);
    SI_SIGNO(SIGXCPU);
    SI_SIGNO(SIGXFSZ);
    SI_SIGNO(SIGVTALRM);
    SI_SIGNO(SIGPROF);
    SI_SIGNO(SIGWINCH);
#ifdef SIGPOLL
    SI_SIGNO(SIGPOLL);
#endif
    SI_SIGNO(SIGIO);
#ifdef SIGPWR
    SI_SIGNO(SIGPWR);
#endif
#ifdef SIGSYS
    SI_SIGNO(SIGSYS);
#endif
#ifdef SIGUNUSED
    SI_SIGNO(SIGUNUSED);
#endif
#undef SI_SIGNO

    return -1; /* not found */
}

static VALUE
si_signo_symbol(int signo)
{
#define SI_SIGNO(v) case v: return ID2SYM(rb_intern(#v));

    switch (signo) {
	SI_SIGNO(SIGHUP);
	SI_SIGNO(SIGINT);
	SI_SIGNO(SIGQUIT);
	SI_SIGNO(SIGILL);
	SI_SIGNO(SIGTRAP);
	SI_SIGNO(SIGABRT);
	/* SI_SIGNO(SIGIOT); dup */
	SI_SIGNO(SIGBUS);
	SI_SIGNO(SIGFPE);
	SI_SIGNO(SIGKILL);
	SI_SIGNO(SIGUSR1);
	SI_SIGNO(SIGSEGV);
	SI_SIGNO(SIGUSR2);
	SI_SIGNO(SIGPIPE);
	SI_SIGNO(SIGALRM);
	SI_SIGNO(SIGTERM);
#ifdef SIGSTKFLT
	SI_SIGNO(SIGSTKFLT);
#endif
	SI_SIGNO(SIGCHLD);
	/* SI_SIGNO(SIGCLD); dup */
	SI_SIGNO(SIGCONT);
	SI_SIGNO(SIGSTOP);
	SI_SIGNO(SIGTSTP);
	SI_SIGNO(SIGTTIN);
	SI_SIGNO(SIGTTOU);
	SI_SIGNO(SIGURG);
	SI_SIGNO(SIGXCPU);
	SI_SIGNO(SIGXFSZ);
	SI_SIGNO(SIGVTALRM);
	SI_SIGNO(SIGPROF);
	SI_SIGNO(SIGWINCH);
#ifdef SIGPOLL
	SI_SIGNO(SIGPOLL);
#endif
	/* SI_SIGNO(SIGIO); dup */
#ifdef SIGPWR
	SI_SIGNO(SIGPWR);
#endif
#ifdef SYGSYS
	SI_SIGNO(SIGSYS);
#endif
	/* SI_SIGNO(SIGUNUSED); dup */
    }

#undef SI_SIGNO

    return INT2FIX(signo); /* realtime signal? */
}

static VALUE
si_code_symbol(int signo, int code)
{

#define SI_CODE(v) case v: return ID2SYM(rb_intern(#v));

    switch (code) {
	SI_CODE(SI_USER);
#ifdef SI_KERNEL
	SI_CODE(SI_KERNEL);
#endif
	SI_CODE(SI_QUEUE);
	SI_CODE(SI_TIMER);
	SI_CODE(SI_MESGQ);
	SI_CODE(SI_ASYNCIO);
#ifdef SI_SIGIO
	SI_CODE(SI_SIGIO);
#endif
#ifdef SI_TKILL
	SI_CODE(SI_TKILL);
#endif
    }

    switch (signo) {
      case SIGILL:
	switch (code) {
	    SI_CODE(ILL_ILLOPC);
	    SI_CODE(ILL_ILLOPN);
	    SI_CODE(ILL_ILLADR);
	    SI_CODE(ILL_ILLTRP);
	    SI_CODE(ILL_PRVOPC);
	    SI_CODE(ILL_PRVREG);
	    SI_CODE(ILL_COPROC);
	    SI_CODE(ILL_BADSTK);
	}
	break;
      case SIGFPE:
	switch (code) {
	    SI_CODE(FPE_INTDIV);
	    SI_CODE(FPE_INTOVF);
	    SI_CODE(FPE_FLTDIV);
	    SI_CODE(FPE_FLTOVF);
	    SI_CODE(FPE_FLTUND);
	    SI_CODE(FPE_FLTRES);
	    SI_CODE(FPE_FLTINV);
	    SI_CODE(FPE_FLTSUB);
	}
	break;
      case SIGSEGV:
	switch (code) {
	    SI_CODE(SEGV_MAPERR);
	    SI_CODE(SEGV_ACCERR);
	}
	break;
      case SIGBUS:
	switch (code) {
	    SI_CODE(BUS_ADRALN);
	    SI_CODE(BUS_ADRERR);
	    SI_CODE(BUS_OBJERR);
	}
	break;
      case SIGTRAP:
	switch (code) {
	    SI_CODE(TRAP_BRKPT);
	    SI_CODE(TRAP_TRACE);
	}
	break;
      case SIGCHLD:
	switch (code) {
	    SI_CODE(CLD_EXITED);
	    SI_CODE(CLD_KILLED);
	    SI_CODE(CLD_DUMPED);
	    SI_CODE(CLD_TRAPPED);
	    SI_CODE(CLD_STOPPED);
	    SI_CODE(CLD_CONTINUED);
	}
	break;
#ifdef SIGPOLL
      case SIGPOLL:
	switch (code) {
	    SI_CODE(POLL_IN);
	    SI_CODE(POLL_OUT);
	    SI_CODE(POLL_MSG);
	    SI_CODE(POLL_ERR);
	    SI_CODE(POLL_PRI);
	    SI_CODE(POLL_HUP);
	}
	break;
#endif
    }

#undef SI_CODE

    return ID2SYM(rb_intern("UNKNOWN_CODE"));
}

#ifdef PT_GETSIGINFO
static VALUE
ptrace_getsiginfo(VALUE self)
{
    pid_t pid = get_pid(self);
    siginfo_t si;
    void *data_ptr = (void *)&si;
    long ret;
    VALUE v;

    CALL_PTRACE(ret, PT_GETSIGINFO, pid, 0, data_ptr);

    v = rb_hash_new();

#define SI_SET(name, val) rb_hash_aset(v, ID2SYM(rb_intern("si_" #name)), val)
    SI_SET(sig, si_signo_symbol(si.si_signo));
    SI_SET(signo, INT2NUM(si.si_signo));
    SI_SET(errno, INT2NUM(si.si_errno));
    SI_SET(code, si_code_symbol(si.si_signo, si.si_code));
    SI_SET(codeno, INT2NUM(si.si_code));

    switch (si.si_signo) {
      case SIGKILL:
	SI_SET(pid, ULONG2NUM(si.si_pid));
	SI_SET(uid, ULONG2NUM(si.si_uid));
	break;

      case SIGILL:
      case SIGFPE:
      case SIGSEGV:
      case SIGBUS:
	SI_SET(addr, ULONG2NUM((unsigned long)si.si_addr));
	break;

      case SIGCHLD:
	SI_SET(pid, ULONG2NUM(si.si_pid));
	SI_SET(uid, ULONG2NUM(si.si_uid));
	SI_SET(status, INT2NUM(si.si_status));
	SI_SET(utime, ULONG2NUM(si.si_utime));
	SI_SET(stime, ULONG2NUM(si.si_stime));
	break;

      case SIGPOLL:
	SI_SET(band, LONG2NUM(si.si_band));
	SI_SET(fd, INT2NUM(si.si_fd));
	break;

      default: /* POSIX.1b signal? */
	;
    }

#undef SI_SET

    return v;
}
#else
static VALUE
ptrace_getsiginfo(VALUE self)
{
    UNSUPPORTED();
    return Qnil;
}
#endif

#if defined(__APPLE__)
static VALUE
ptrace_setregs(VALUE self, VALUE data)
{
    x86_thread_state_t current_thread_state;
    VALUE ret = Qnil;
    int state_count = x86_THREAD_STATE_COUNT; /* only x86 supported */
    kern_return_t thread_status = thread_get_status(pthread_self(),
                                                    x86_THREAD_STATE,
                                                    &current_thread_state,
                                                    &state_count);
    if ( thread_status != 0 )
    {
        rb_raise(rb_ePTraceError, "thread_get_state failed with error code: %d",
                 thread_status);
    }
    
#define SET(reg) {VALUE v = rb_struct_getmember(data, rb_intern(#reg)); \
        current_thread_state.uts.ts64.__##reg = NUM2ULONG(v);}
    SET(rax);
    SET(rbx);
    SET(rcx);
    SET(rdi);
    SET(rsi);
    SET(rbp);
    SET(rsp);
    SET(r8);
    SET(r9);
    SET(r10);
    SET(r11);
    SET(r12);
    SET(r13);
    SET(r14);
    SET(r15);
    SET(rip);
    SET(rflags);
    SET(cs);
    SET(fs);
    SET(gs);
    kern_return_t set_thread_status = thread_set_state(pthread_self(),
                                                       x86_DEBUG_STATE,
                                                       &current_thread_state,
                                                       x86_DEBUG_STATE_COUNT);
    if ( set_thread_status != 0 )
    {
        rb_raise(rb_ePTraceError, "thread_set_state failed with error code: %d",
                 set_thread_status);
    }
    return Qnil;
}
#else
#ifdef PT_SETREGS
static VALUE
ptrace_setregs(VALUE self, VALUE data)
{
    struct user_regs_struct urs;
    void *data_ptr = (void *)&urs;
    pid_t pid = get_pid(self);
    long ret;

#define SET(reg) {VALUE v = rb_struct_getmember(data, rb_intern(#reg)); urs.reg = NUM2ULONG(v);}
    SET(ebx);
    SET(ecx);
    SET(edx);
    SET(esi);
    SET(edi);
    SET(ebp);
    SET(eax);
    SET(xds);
    SET(xes);
    SET(xfs);
    SET(xgs);
    SET(orig_eax);
    SET(eip);
    SET(xcs);
    SET(eflags);
    SET(esp);
    SET(xss);
#undef SET

    CALL_PTRACE(ret, PT_SETREGS, pid, 0, data_ptr);
    return Qnil;
}
#else  /* PT_SETREGS */
static VALUE
ptrace_setregs(VALUE self, VALUE data)
{
    UNSUPPORTED();
    return Qnil;
}
#endif  /* PT_SETREGS */
#endif  /* __APPLE__ */
static VALUE
ptrace_setfpregs(VALUE self, VALUE data)
{
    UNSUPPORTED();
    return Qnil;
}

static VALUE
ptrace_continue(VALUE self, ptrace_request request, VALUE data)
{
    pid_t pid = get_pid(self);
    long ret;
    long sig = 0;

    if (FIXNUM_P(data)) {
	sig = FIX2LONG(data);
    }
    else if (SYMBOL_P(data)) {
	sig = signo_symbol_to_int(data);
    }
    else {
	rb_raise(rb_eRuntimeError, "unknown data");
    }

    CALL_PTRACE(ret, request, pid, 0, (void *)sig);
    return Qnil;
}

static VALUE
ptrace_cont(int argc, VALUE *argv, VALUE self)
{
    VALUE data = INT2FIX(0);
    if (argc == 1) {
	data = argv[0];
    }
    return ptrace_continue(self, PT_CONTINUE, data);
}

#ifdef PT_SYSCALL
static VALUE
ptrace_syscall(int argc, VALUE *argv, VALUE self)
{
    VALUE data = INT2FIX(0);
    if (argc == 1) {
	data = argv[0];
    }
    return ptrace_continue(self, PT_SYSCALL, data);
}
#else
static VALUE
ptrace_syscall(int argc, VALUE *argv, VALUE self)
{
    UNSUPPORTED();
    return Qnil;
}
#endif

static VALUE
ptrace_singlestep(int argc, VALUE *argv, VALUE self)
{
    VALUE data = INT2FIX(0);
    if (argc == 1) {
	data = argv[0];
    }
    return ptrace_continue(self, PT_STEP, data);
}

#ifdef HAVE_PTRACE_SYSEMU
static VALUE
ptrace_sysemu(int argc, VALUE *argv, VALUE self)
{
    VALUE data = INT2FIX(0);
    if (argc == 1) {
	data = argv[0];
    }
    return ptrace_continue(self, PTRACE_SYSEMU, data);
}
#endif

#ifdef HAVE_PTRACE_SYSEMU_SINGLESTEP
static VALUE
ptrace_sysemu_singlestep(int argc, VALUE *argv, VALUE self)
{
    VALUE data = INT2FIX(0);
    if (argc == 1) {
	data = argv[0];
    }
    return ptrace_continue(self, PTRACE_SYSEMU_SINGLESTEP, data);
}
#endif

static VALUE
ptrace_kill(VALUE self)
{
    pid_t pid = get_pid(self);
    long ret;
    CALL_PTRACE(ret, PT_KILL, pid, 0, 0);
    return Qnil;
}

static VALUE
ptrace_wait(VALUE self)
{
    pid_t pid = get_pid(self);
    int st;
    int ret = rb_waitpid(pid, &st, 0);

    if (ret == -1) {
	rb_sys_fail("waitpid(2)");
    }

    if (WIFSTOPPED(st)) {
	return si_signo_symbol(WSTOPSIG(st));
    }
    return Qnil;
}

static VALUE
ptrace_detach(VALUE self)
{
    pid_t pid = get_pid(self);
    long ret;
    CALL_PTRACE(ret, PT_DETACH, pid, 0, 0);
    rb_ivar_set(self, id_ptrace_pid, Qnil);
    return Qnil;
}

static VALUE
ptrace_alloc(VALUE mod, pid_t pid)
{
    VALUE v = rb_obj_alloc(mod);
    rb_ivar_set(v, id_ptrace_pid, LONG2NUM(pid));
    return v;
}

static VALUE
ptrace_attach(VALUE mod, VALUE pidv)
{
    pid_t pid = NUM2LONG(pidv);
    long ret;
    CALL_PTRACE(ret, PT_ATTACH, pid, 0, 0);
    return ptrace_alloc(mod, pid);
}

static VALUE
ptrace_traceme(VALUE mod)
{
    long ret = ptrace(PT_TRACE_ME, 0, 0, 0);

    if (ret == -1) {
	fprintf(stderr, "ptrace (PTRACE_TRACEME) error (%s:%d): %ld\n",
		__FILE__, __LINE__, ret);
    }

    return mod;
}

static VALUE
ptrace_exec(int argc, VALUE *argv, VALUE mod)
{
    pid_t pid = fork();

    if (pid == 0) {
	/* child */
	ptrace_traceme(Qnil);

	if (rb_f_exec(argc, argv) == Qnil) {
	    fprintf(stderr, "exec error (%s:%d)\n", __FILE__, __LINE__);
	}
    }
    else if (pid == -1) {
	rb_sys_fail("fork(2)");
    }

    return ptrace_alloc(mod, pid);
}

static VALUE
ptrace_pid(VALUE self)
{
    return LONG2NUM(get_pid(self));
}

static VALUE
ptrace_set_pid(VALUE self, VALUE pidv)
{
    rb_ivar_set(self, id_ptrace_pid, pidv);
    return self;
}

void
Init_ptrace(void)
{
    VALUE klass = rb_define_class("PTrace", rb_cObject);
    rb_define_method(klass, "peektext", ptrace_peektext, 1);
    rb_define_method(klass, "peekdata", ptrace_peekdata, 1);
    rb_define_method(klass, "peekuser", ptrace_peekuser, 1);
    rb_define_method(klass, "poketext", ptrace_poketext, 2);
    rb_define_method(klass, "pokedata", ptrace_pokedata, 2);
    rb_define_method(klass, "pokeuser", ptrace_pokeuser, 2);
    rb_define_method(klass, "getregs",  ptrace_getregs, 0);
    rb_define_method(klass, "getfpregs", ptrace_getfpregs, 0);
    rb_define_method(klass, "getsiginfo", ptrace_getsiginfo, 0);
    rb_define_method(klass, "setregs",  ptrace_setregs, 1);
    rb_define_method(klass, "setfpregs", ptrace_setfpregs, 1);
    rb_define_method(klass, "cont", ptrace_cont, -1);
    rb_define_method(klass, "syscall", ptrace_syscall, -1);
    rb_define_method(klass, "singlestep", ptrace_singlestep, -1);
#ifdef HAVE_PTRACE_SYSEMU
    rb_define_method(klass, "sysemu", ptrace_sysemu, -1);
#endif
#ifdef HAVE_PTRACE_SYSEMU_SINGLESTEP
    rb_define_method(klass, "sysemu_singlestep", ptrace_sysemu_singlestep, -1);
#endif
    rb_define_method(klass, "kill", ptrace_kill, 0);
    rb_define_method(klass, "wait", ptrace_wait, 0);
    rb_define_method(klass, "detach", ptrace_detach, 0);
    rb_define_method(klass, "pid", ptrace_pid, 0);
    rb_define_method(klass, "__set_pid__", ptrace_set_pid, 1);

    rb_define_singleton_method(klass, "attach", ptrace_attach, 1);
    rb_define_singleton_method(klass, "exec", ptrace_exec, -1);
    rb_define_singleton_method(klass, "traceme", ptrace_traceme, 0);

    id_ptrace_pid = rb_intern("__ptrace_pid__");
    rb_ePTraceError = rb_define_class("PTraceError", rb_eStandardError);

#if defined(__APPLE__)
    rb_sPTraceRegStruct =
        rb_struct_define("RegStruct",
                         "rax", "rbx", "rcx", "rdi", "rsi", "rbp", "rsp", "r8",
                         "r9", "r10", "r11", "r12", "r13", "r14", "r15", "rip",
                         "rflags", "cs", "fs", "gs",
                         0);
#else  /* linux */
    rb_sPTraceRegStruct =
      rb_struct_define("RegStruct",
		       "ebx", "ecx", "edx", "esi", "edi", "ebp", "eax", "xds",
		       "xes", "xfs", "xgs", "orig_eax", "eip", "xcs",
		       "eflags", "esp", "xss", 0);
#endif
    
    rb_sPTraceFPRegStruct =
      rb_struct_define("FPRegStruct",
		       "cwd", "swd", "twd", "fop", "fip", "fcs", "foo",
		       "fos", "mxcsr", "reserved", "st_space", "xmm_space",
		       "padding", 0);
}

