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
#include "opt-A3.h"
#if OPT_A2
#include <kern/fcntl.h>
#include <vm.h>
#include <vfs.h>
#endif
#if OPT_A3
void sys_kill(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  handleChildrenOnDeath(p);


  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(p->p_addrspace != NULL);

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

  struct proc *parent_proc = p->parent_process;


  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  struct cv *curproc_cv = p->process_cv;
  lock_acquire(process_lock);
  if (parent_proc != NULL) {
		//lock_acquire(process_lock);
    p->EXIT_CODE = _MKWAIT_SIG(exitcode);
    p->isAlive = false;

  }
  proc_remthread(curthread);
  cv_broadcast(curproc_cv, process_lock);
  lock_release(process_lock);
  //cv_broadcast(p->process_cv, process_lock);
  if (parent_proc == NULL) {
    proc_destroy(p);
  }
  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */

  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");

}

#endif

void sys__exit(int exitcode) {

	struct addrspace *as;
	struct proc *p = curproc;
	handleChildrenOnDeath(p);
	

	DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

	KASSERT(p->p_addrspace != NULL);

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

	struct proc *parent_proc = p->parent_process;
 

	/* detach this thread from its process */
	/* note: curproc cannot be used after this call */
	struct cv *curproc_cv = p->process_cv;
	lock_acquire(process_lock);
	if (parent_proc != NULL) {
		//lock_acquire(process_lock);
		p->EXIT_CODE = _MKWAIT_EXIT(exitcode);
		p->isAlive = false;
		
	}
	proc_remthread(curthread);
	cv_broadcast(curproc_cv, process_lock);
	lock_release(process_lock);
	//cv_broadcast(p->process_cv, process_lock);
	if (parent_proc == NULL) {
    proc_destroy(p); 
  }
	/* if this is the last user process in the system, proc_destroy()
		 will wake up the kernel menu thread */

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


  if (options != 0) {
    return(EINVAL);
  }
  /* for now, just pretend the exitstatus is 0 */
#if OPT_A2	
	//*retval = 1;
	if (inProcessList(pid)) {
		lock_acquire(process_lock);
		struct proc *child_proc = getChild(curproc, pid);
		if(child_proc != NULL) {
			if (child_proc->isAlive) {
				//DEBUG(DB_EXEC, "\nGoing to Sleep\n");
				cv_wait(child_proc->process_cv, process_lock); 
				//DEBUG(DB_EXEC, "\nWoke Up\n");
			} 
			//DEBUG(DB_EXEC, "\nGetting Error Code\n");
			exitstatus = child_proc->EXIT_CODE;
			//DEBUG(DB_EXEC, "\nReleasing lock\n");
			//lock_release(process_lock);
		}
		else {
			return ECHILD;
		}
		lock_release(process_lock);		
	}
	else {
		return ESRCH;
	}	
#endif
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
	*retval = 1;
	struct proc* child_process;
	//kprintf("Creating a process WOOHOO\n");
	child_process = proc_create_runprogram("1");

	if (child_process == NULL) {
		
		return ENOMEM;
	}
	lock_acquire(child_process->proc_lock);
	int result_of_ascopy = as_copy(curproc_getas(), &child_process->p_addrspace);
	if (result_of_ascopy != 0) {
		lock_release(child_process->proc_lock);
		proc_destroy(child_process);
		//DEBUG(DB_EXEC, "\nlock 1\n");
		return ENOMEM;
	}
	//DEBUG(DB_EXEC, "\nlock 2\n");
	lock_release(child_process->proc_lock);
	//DEBUG(DB_EXEC, "\nlock 3\n");
  handlePIDpcrelationship(curproc, child_process);

	struct trapframe *copytrapframe = kmalloc(sizeof (struct trapframe));
	if (copytrapframe == NULL) {

		proc_destroy(child_process);
		return ENOMEM;
	}
	*copytrapframe = *tf;  	

	int threadfork_errorcode = -1;
	threadfork_errorcode = thread_fork("thread_of_child", child_process, &enter_forked_process, (void *)copytrapframe, (unsigned long)2);

	if (threadfork_errorcode != 0) {
		proc_destroy(child_process);
		return ENOMEM;
	}

	*retval = child_process->self_pid;	
	//kfree(copytrapframe);
	return 0;
}

int
sys_execv(userptr_t program_name, userptr_t oldas_args)
{
	
	if (program_name == NULL) {
		return EFAULT;
	}
		
	size_t checklen = 0;
	int prognamelength = strlen((char *)program_name) + 1;
	char *progname = kmalloc(sizeof(char) * prognamelength);
	

	//check for failure of kmalloc
	if (progname == NULL) {
		return ENOMEM;
 	}
	//copy programname from previous addresspace to Kernel
	int progname_copy_result = copyinstr((const_userptr_t)program_name, progname, (size_t)prognamelength, &checklen); 
	if (progname_copy_result) {
		return progname_copy_result;
	}
	//kprintf("Program name passed to execv is %s\n", progname);

	//cast oldas_args which is a userptr_t to char**
	char ** args = (char **)oldas_args;
	int args_counter = 0;

	//count the number of arguments in args
	while (args[args_counter] != NULL) {
		//kprintf("Argument %d is %s\n", args_counter,args[args_counter]);
		++args_counter;
	}
	
	//kprintf("Argument %d is %s\n", args_counter, args[args_counter]);
	//kprintf("No of arguments passed are %d\n", args_counter);
	
	char **kargs = kmalloc(sizeof (char *) * (args_counter + 1));

	if (kargs == NULL) {
		return ENOMEM;
	}

	for (int i = 0; i < args_counter; ++i) {
		int arglength = strlen(args[i]) + 1;
		kargs[i] = kmalloc(sizeof(char) * arglength);
		if (kargs[i] == NULL) {
			return ENOMEM;
		}
		int arg_copy_result = copyinstr((const_userptr_t)args[i], kargs[i], (size_t)arglength, &checklen);
		if (arg_copy_result) {
			return arg_copy_result;
		} 
		//kprintf("Argument %d in kernel is %s\n", i,kargs[i]);
	}
	kargs[args_counter] = NULL;

	//part of runprogram
	struct addrspace *as;

  struct vnode *v;
  vaddr_t entrypoint, stackptr;
  int result;

  /* Open the file. */
  result = vfs_open(progname, O_RDONLY, 0, &v);
  if (result) {
    return result;
  }


  /* Create a new address space. */
  as = as_create();
  if (as ==NULL) {
    vfs_close(v);
    return ENOMEM;
  }

  /* Switch to it and activate it. */
  curproc_setas(as);
  as_activate();

  /* Load the executable. */
  result = load_elf(v, &entrypoint);
  if (result) {
    /* p_addrspace will go away when curproc is destroyed */
    vfs_close(v);
    return result;
  }

  /* Done with the file now. */
  vfs_close(v);

  /* Define the user stack in the address space */
  result = as_define_stack(as, &stackptr);
  if (result) {
    /* p_addrspace will go away when curproc is destroyed */
    return result;
  }
	
	//creating a copy of stackptr
	//vaddr_t stackptrcopy = stackptr;

	//decreasing stackptr by 1 so that its not 0x8000 0000 as we can not write to that address
	stackptr = stackptr - 1;

	//create an array of vaddr_t to track the vaddr of the arguments that we are putting onto the stack
	vaddr_t args_vaddr[args_counter + 1];

	//starting to set stack for the new address space
	for (int i = args_counter - 1; i >= 0 ; --i) {
		int arglength = strlen(kargs[i]) + 1;
		stackptr = stackptr - arglength;
		int arg_copy_result = copyoutstr(kargs[i], (userptr_t)stackptr, arglength, &checklen);
		if (arg_copy_result) {
			return arg_copy_result;
		}
		//store the starting address of where we wrote the string
		args_vaddr[i] = stackptr;
	}

	//kargs[args_counter] is NULL whose vaddr is 0x0
	args_vaddr[args_counter] = 0;

	//now we have the args in the stack of the new address space we need to have the pointers to these args
	//first get to an address divisible by 4
	while ((stackptr % 4) != 0) {
		stackptr = stackptr - 1;
	}

	//now we can start storing the vaddr of the arguments that we already have been tracking in args_vaddr
	for (int i = args_counter; i >= 0; --i) {
		int arg_size = ROUNDUP(sizeof(vaddr_t), 4);
		stackptr = stackptr - arg_size;
		int arg_vaddr_copy_result = copyout(&args_vaddr[i], (userptr_t)stackptr, sizeof(vaddr_t));
		if (arg_vaddr_copy_result) {
			return arg_vaddr_copy_result;
		}
	}

  /* Warp to user mode. */
  enter_new_process(args_counter, (userptr_t)stackptr /*userspace addr of argv*/,
        stackptr, entrypoint);

  /* enter_new_process does not return. */
  panic("enter_new_process returned\n");
  return EINVAL;
}

#endif
