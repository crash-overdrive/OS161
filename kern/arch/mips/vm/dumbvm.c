/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include "opt-A3.h"
/*
 * Dumb MIPS-only "VM system" that is intended to only be just barely
 * enough to struggle off the ground.
 */

/* under dumbvm, always have 48k of user stack */
#define DUMBVM_STACKPAGES    12

/*
 * Wrap rma_stealmem in a spinlock.
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

#if OPT_A3

void alloc_coremap(void);

int *coremap;
paddr_t firstFrameAddr;
int noOfFrames;
bool coreFormed = false;

struct pageTableEntry {
  paddr_t frameBasePhysAddr;
};

void alloc_coremap(void) {
	paddr_t minPhysAddr = 0;
	paddr_t maxPhysAddr = 0;

	//first get the minPhysAddr and maxPhysAddr
	ram_getsize(&minPhysAddr, &maxPhysAddr);

	//finding possible no of Frames 
	noOfFrames = (maxPhysAddr - minPhysAddr) / PAGE_SIZE;	

	//then steal memory for coremap 
	coremap = kmalloc(sizeof (int) * noOfFrames);	

	//get info about firstFrameAddr that is going to be allocated by the kernel
	ram_getsize(&firstFrameAddr, &maxPhysAddr);

	//round up the firstFrameAddr so that it is a multiple of PAGE_SIZE
	while(firstFrameAddr % PAGE_SIZE != 0) {
		firstFrameAddr = firstFrameAddr + 1;
	}

	//initialize coremap to be all empty
	for (int i = 0; i < noOfFrames; ++i) {

		coremap[i] = 0; 

		if (firstFrameAddr + i * PAGE_SIZE > maxPhysAddr) {
			//some issues might arise because of coremap needing multiple pages, this block takes care of it
			//since we never want to access these out of bounds entries we can just reduce noOfFrames! Neat :D
			noOfFrames = noOfFrames - 1;
			kprintf("Out of bounds memory found during coremap initialisation, reduce noOfFrames \n");
		}		
	}
	
	//set coreFormed to be true as coremap is now formed and ready to go!
	coreFormed = true;

	//finally switch to coremap and start using it!
	switchTocoremap();
}

#endif //OPT_A3

void
vm_bootstrap(void)
{
	#if OPT_A3
	alloc_coremap();	
	#endif
}


static
paddr_t
getppages(unsigned long npages)
{
	paddr_t addr;
	spinlock_acquire(&stealmem_lock);
	#if OPT_A3
	if (!coreFormed) {
		addr = ram_stealmem(npages);
	}
	else {

		int noOfPagesRequired = (int)npages;
		//kprintf("Allocate: %d frames\n", noOfPagesRequired);
		bool memBlockFound = false;
		int startingFrame;

		for (int i = 0; i < noOfFrames; ++i) {
			if(coremap[i] == 0) { //checking if coremap at i is empty

				startingFrame = i;
				int noOfPagesFound = 1;

				if (noOfPagesRequired > 1) { //checking if we need more than 1 Frame to allocate the Pages
					for (int j = i + 1; j < noOfFrames; ++j) {
						if (coremap[j] == 0) { //checking if coremap at j is empty

							++noOfPagesFound;

							if (noOfPagesFound == noOfPagesRequired) { //checking if we hit the target of noOfPagesRequired
								memBlockFound = true;
								break;
							}
						}
						else { //uncontiguous memory detected (coremap[j] != 0), have to restart the search at j+1 :(
							i = j + 1;
							break;
						}
					}
				}
				else { //found 1 block of memory when we needed only 1 block of memory!!
					memBlockFound = true;
				}
				if (memBlockFound) { //if memBlockFound then we can exit outer loop
					break;
				}
			}
		}
		if (memBlockFound) {
			for (int i = 0; i < noOfPagesRequired; ++i) {
				coremap[startingFrame + i] = i + 1;
				//kprintf("Allocate: Set frame %d with address: 0x%x to %d\n", startingFrame + i,firstFrameAddr + ((startingFrame+i) * PAGE_SIZE) , i+1);
			}
			addr = firstFrameAddr + (startingFrame * PAGE_SIZE);
		}
		else {
			spinlock_release(&stealmem_lock);
			kprintf("Ran out of memory trying to allocate %lu frames\n", npages);
			for (int i = 0; i < noOfFrames; ++i) {
				kprintf("Frame Number: %d, Frame Value: %d\n", i, coremap[i]);
			}
			return ENOMEM;
		}
	}
	#endif //OPT_A3
	spinlock_release(&stealmem_lock);
	return addr;
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t 
alloc_kpages(int npages)
{
	paddr_t pa;
	pa = getppages(npages);
	if (pa==0) {
		return 0;
	}
	#if OPT_A3
	else if(pa == ENOMEM) {
		return ENOMEM;
	}
	#endif //OPT_A3
	return PADDR_TO_KVADDR(pa);
}

void 
free_kpages(vaddr_t addr)
{
	#if OPT_A3
	spinlock_acquire(&stealmem_lock);
	if (coreFormed) {
		if (!addr) {
			kprintf("Error while freeing");
			spinlock_release(&stealmem_lock);
			return;
		}
		int currentFrame = (addr - firstFrameAddr)/PAGE_SIZE;
		if (currentFrame >= noOfFrames) { //got a stack address
			addr = PADDR_TO_KVADDR(addr);
			//kprintf("got stack address, normal address is 0x%x\n", addr);
			currentFrame = (addr - firstFrameAddr)/PAGE_SIZE;
		}
		int previousFrameStatus = coremap[currentFrame];
		//KASSERT(previousFrameStatus == 1);
		coremap[currentFrame] = 0;
		//kprintf("Delete: Set First Frame %d to 0\n", currentFrame);
		while (currentFrame + 1 < noOfFrames) {
			currentFrame = currentFrame + 1;
			int currentFrameStatus = coremap[currentFrame];
			if (currentFrameStatus == previousFrameStatus + 1) {
				previousFrameStatus = currentFrameStatus;
				coremap[currentFrame] = 0;
				//kprintf("Delete: Set Frame %d to 0\n", currentFrame);
			}
			else {
				//kprintf("Delete: Set Last Frame %d to 0\n", currentFrame - 1);
				break;
			}
		}
	}
		
	spinlock_release(&stealmem_lock);
	#endif
}

void
vm_tlbshootdown_all(void)
{
	panic("dumbvm tried to do tlb shootdown?!\n");
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	int i;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;
	bool isCodeSegment = false;

	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
		/* We always create pages read-write, so we can't get this */
		//panic("dumbvm: got VM_FAULT_READONLY\n");
				return 1;
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		return EINVAL;
	}

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	as = curproc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */
	KASSERT(as->as_vbase1 != 0);
	//KASSERT(as->as_pbase1 != 0);
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_vbase2 != 0);
	//KASSERT(as->as_pbase2 != 0);
	KASSERT(as->as_npages2 != 0);
	//KASSERT(as->as_stackpbase != 0);
	KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	//KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
	KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
	//KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
	//KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);

	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

	//TODO
	if (faultaddress >= vbase1 && faultaddress < vtop1) {

		vaddr_t offset = (faultaddress - vbase1);
        vaddr_t pageNum = offset / PAGE_SIZE;
        paddr = as->as_pageTable1[pageNum].frameBasePhysAddr;		
	
		isCodeSegment = true;
	}
	else if (faultaddress >= vbase2 && faultaddress < vtop2) {
		vaddr_t offset = (faultaddress - vbase2);
        vaddr_t pageNum = offset / PAGE_SIZE;
        paddr = as->as_pageTable2[pageNum].frameBasePhysAddr;	
	}
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
		vaddr_t offset = (faultaddress - stackbase);
        vaddr_t pageNum = offset / PAGE_SIZE;
        paddr = as->as_pageTableStack[pageNum].frameBasePhysAddr;	
	}
	else {
		return EFAULT;
	}

	/* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		#if OPT_A3
		if ((as->loadelfcompleted == true) && (isCodeSegment)) {
			elo &= ~TLBLO_DIRTY;
		}
		#endif
		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}
	#if OPT_A3
	//kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
	ehi = faultaddress;
	elo = paddr|TLBLO_DIRTY | TLBLO_VALID;
	if ((as->loadelfcompleted == true) && (isCodeSegment)) {
    elo &= ~TLBLO_DIRTY;
  } 
	tlb_random(ehi,elo);
	splx(spl);
	return 0;
	#endif
}

struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}

	as->as_vbase1 = 0;
	//as->as_pbase1 = 0;
	as->as_npages1 = 0;
	as->as_vbase2 = 0;
	//as->as_pbase2 = 0;
	as->as_npages2 = 0;
	//as->as_stackpbase = 0;
	#if OPT_A3
	as->loadelfcompleted = false;
	as->as_pageTable1 = NULL;
	as->as_pageTable2 = NULL;
	as->as_pageTableStack = NULL;
	#endif
	return as;
}

void
as_destroy(struct addrspace *as)
{
	#if OPT_A3
	
	for (int i = 0; i < (int)as->as_npages1; ++i) {
		free_kpages(as->as_pageTable1[i].frameBasePhysAddr);
	}

	for (int i = 0; i < (int)as->as_npages2; ++i) {
		free_kpages(as->as_pageTable2[i].frameBasePhysAddr);
	}

	for (int i = 0; i < DUMBVM_STACKPAGES; ++i) {
		free_kpages(as->as_pageTableStack[i].frameBasePhysAddr);
	}
	
	
	kfree(as->as_pageTable1);
	kfree(as->as_pageTable2);
	kfree(as->as_pageTableStack);
	kfree(as);

	spinlock_acquire(&stealmem_lock);
	/*kprintf("Leak check begins\n");
	for (int i = 0; i < noOfFrames; ++i) {
		
		if(coremap[i] != 0) {
			kprintf("Leaked Memory at frame %d has value %d\n",i, coremap[i]);
		}
		
	}
	kprintf("Leak check ends\n");
	*/
	spinlock_release(&stealmem_lock);

	
	#endif
}

void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = curproc_getas();
#ifdef UW
        /* Kernel threads don't have an address spaces to activate */
#endif
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	/* nothing */
}

int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	size_t npages; 

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;
	
	#if OPT_A3
		if (readable) {
			as->as_readable = 1;
		}
		else {
			as->as_readable = 0;
		}
		if (writeable) {
			as->as_writeable = 1;
		}
		else {
			as->as_writeable = 0;
		}
		if (executable) {
			as->as_executable = 1;
		}
		else {
			as->as_executable = 0;
		}

	#endif
	
	if (as->as_vbase1 == 0) {
		as->as_vbase1 = vaddr;
		as->as_npages1 = npages;
		return 0;
	}

	if (as->as_vbase2 == 0) {
		as->as_vbase2 = vaddr;
		as->as_npages2 = npages;
		return 0;
	}

	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("dumbvm: Warning: too many regions\n");
	return EUNIMP;
}

static
void
as_zero_region(paddr_t paddr, unsigned npages)
{
	bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
}

int
as_prepare_load(struct addrspace *as)
{	
	//allocationg memory for the page table entries for the address space
	as->as_pageTable1 = kmalloc(sizeof(struct pageTableEntry) * (int)as->as_npages1);
	if(as->as_pageTable1 == NULL){
        return ENOMEM;
    }

	as->as_pageTable2 = kmalloc(sizeof(struct pageTableEntry) * (int)as->as_npages2);
	if(as->as_pageTable2 == NULL){
        return ENOMEM;
    }

	as->as_pageTableStack = kmalloc(sizeof(struct pageTableEntry) * DUMBVM_STACKPAGES);
	if(as->as_pageTableStack == NULL){
        return ENOMEM;
    }


	paddr_t pbase1, pbase2, pstackbase;
	for (int i = 0; i < (int)as->as_npages1; ++i) {
		pbase1 = getppages(1);

		if (pbase1 == 0) {
			return ENOMEM;
		}		

		as->as_pageTable1[i].frameBasePhysAddr = pbase1;

		as_zero_region(pbase1, 1);
		
	}
	for (int i = 0; i < (int)as->as_npages2; ++i) {
		pbase2 = getppages(1);

		if (pbase2 == 0) {
			return ENOMEM;
		}		

		as->as_pageTable2[i].frameBasePhysAddr = pbase2;

		as_zero_region(pbase2, 1);
	}
	for (int i = 0; i < (int)DUMBVM_STACKPAGES; ++i) {
		pstackbase = getppages(1);

		if (pstackbase == 0) {
			return ENOMEM;
		}		

		as->as_pageTableStack[i].frameBasePhysAddr = pstackbase;

		as_zero_region(pstackbase, 1);
	}

	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	//KASSERT(as->as_stackpbase != 0);
	(void)as;
	*stackptr = USERSTACK;
	return 0;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *new;

	new = as_create();
	if (new==NULL) {
		return ENOMEM;
	}

	new->as_vbase1 = old->as_vbase1;
	new->as_npages1 = old->as_npages1;
	new->as_vbase2 = old->as_vbase2;
	new->as_npages2 = old->as_npages2;

	

	/* (Mis)use as_prepare_load to allocate some physical memory. */
	if (as_prepare_load(new)) {
		as_destroy(new);
		return ENOMEM;
	}

	for (int i = 0; i < (int)new->as_npages1; ++i) {
		memmove((void *)PADDR_TO_KVADDR(new->as_pageTable1[i].frameBasePhysAddr),
				(const void *)PADDR_TO_KVADDR(old->as_pageTable1[i].frameBasePhysAddr),
				PAGE_SIZE);
	}

	for (int i = 0; i < (int)new->as_npages2; ++i) {
		memmove((void *)PADDR_TO_KVADDR(new->as_pageTable2[i].frameBasePhysAddr),
				(const void *)PADDR_TO_KVADDR(old->as_pageTable2[i].frameBasePhysAddr),
				PAGE_SIZE);
	}

	for (int i = 0; i < DUMBVM_STACKPAGES; ++i) {
		memmove((void *)PADDR_TO_KVADDR(new->as_pageTableStack[i].frameBasePhysAddr),
				(const void *)PADDR_TO_KVADDR(old->as_pageTableStack[i].frameBasePhysAddr),
				PAGE_SIZE);
	}
	
	*ret = new;
	return 0;
}
