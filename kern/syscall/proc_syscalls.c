#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include "opt-A2.h"
#include "mips/trapframe.h" 
  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */




void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  *retval = curproc->self_pid;
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

#if OPT_A2
int 
sys_fork(struct trapframe *tf, int *retval) {

	struct proc* child_process;
	child_process = proc_create_runprogram("1");

	if (child_process == NULL) {
		return ENOMEM;
	}
	spinlock_acquire(&child_process->p_lock);
	int result_of_ascopy = as_copy(curproc_getas(), &child_process->p_addrspace);
	if (result_of_ascopy != 0) {
		proc_destroy(child_process);
		return ENOMEM;
	}
	spinlock_release(&child_process->p_lock);


	struct trapframe *copytrapframe = kmalloc(sizeof (struct trapframe));
	if (copytrapframe == NULL) {

		proc_destroy(child_process);
		return ENOMEM;
	}
	handlePIDpcrelationship(curproc, child_process);
	*copytrapframe = *tf;  	
	int threadfork_errorcode = -1;
	threadfork_errorcode = thread_fork("thread_of_child", child_process, &enter_forked_process, (void *)copytrapframe, (unsigned long)2);
	if (threadfork_errorcode != 0) {
		proc_destroy(child_process);
		return ENOMEM;
	}
	*retval = child_process->self_pid;	
	return 0;
}
#endif
