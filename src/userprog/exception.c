#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"

int find_free_frame();
int evict_page();
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);
static void frame_insert(int free_frame, struct sup_page_table_entry *);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      thread_exit (); 

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      thread_exit ();
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */

  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));
  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();
  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;

  if(fault_addr==NULL)
  {
  	f->eax=-1;
  	thread_current()->parent->return_value=f->eax;
  	printf("%s: exit(%d)\n",thread_name(),f->eax);
	thread_exit();
  }
  if(!is_user_vaddr(fault_addr))
  {  
  	f->eax=-1;
  	thread_current()->parent->return_value=f->eax;
  	printf("%s: exit(%d)\n",thread_name(),f->eax);
	thread_exit();
  }
  bool handled = false;
  struct list_elem *e;
  uint8_t *kpage;
  int free_frame;
  int j;
  struct disk *d;
  for (e = list_begin (&(thread_current()->sup_page_table)); e != list_end (&(thread_current()->sup_page_table)); e = list_next (e))
  {
  	struct sup_page_table_entry *spte = list_entry (e, struct sup_page_table_entry, elem);
  	if(spte->va == (int*)((int)fault_addr & 0xfffff000))
  	{
  		switch(spte->type)
  		{
  			case 0:
  				break;
  			case 1:      
  				kpage = palloc_get_page (PAL_USER);
			       	if (kpage == NULL)
			       	{
			    free_frame = find_free_frame();
					if(free_frame != -1)
					{
						frame_insert(free_frame,spte);
					}
					else
					{
						free_frame = evict_page();
						frame_insert(free_frame,spte);
					}
			       	}
			      	else
			      	{
			      		file_seek (spte->file, spte->offset);
			      		int frame_index = ((vtop(kpage) & 0xfffff000)>>12);
			      		if (file_read (spte->file, kpage, spte->page_read_bytes) != (int) spte->page_read_bytes)
					{
				  		palloc_free_page (kpage);
				  		return;
					}
					 		memset (kpage + spte->page_read_bytes, 0, spte->page_zero_bytes);
			    
				   
				      	if (!install_page (spte->va, kpage, spte->writable)) 
					{
					  	palloc_free_page (kpage);
					  	return;
					}
					frames[frame_index].pid = thread_current()->tid;
			       		frames[frame_index].pte = spte->va;
			      		frames[frame_index].free_bit = false;
			      		frames[frame_index].kpage = kpage;
				}
				spte->type = 0;
				handled = true;
				break;
			case 2:
				kpage = palloc_get_page (PAL_USER);
				if(kpage == NULL)
				{
					free_frame = find_free_frame();
							if(free_frame != -1)
					{
						frame_insert(free_frame,spte);
					}
					else
					{
							free_frame = evict_page();
							frame_insert(free_frame,spte);
					}
				}
				else
				{
			
					int frame_index = ((vtop(kpage) & 0xfffff000)>>12);
			      		memset(kpage,0,spte->page_zero_bytes);
				      	if (!install_page (spte->va, kpage, spte->writable)) 
					{
					  	palloc_free_page (kpage);
					  	return;
					}
					frames[frame_index].pid = thread_current()->tid;
			       		frames[frame_index].pte = spte->va;
			      		frames[frame_index].free_bit = false;
			      		frames[frame_index].kpage = kpage;
				}
				spte->type = 0;
				handled = true;
				break;
			case 3:
					free_frame = find_free_frame();
					if(free_frame != -1)
					{
						frame_insert(free_frame,spte);
					}
					else
					{
						free_frame = evict_page();
						frame_insert(free_frame,spte);
					}
					spte->type = 0;
					handled = true;
					break;
			default:
				NOT_REACHED();
				break;
  		}
  		if(handled)
  			break;
  	}
  }
  if(!handled)
  {
  	if(fault_addr >= ((f->esp)-32))
  	{
  		uint8_t *kpage;
  		bool success = false;
  		kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  		if (kpage != NULL) 
    		{
      			success = install_page ((void*)((int)fault_addr & 0xfffff000), kpage, true);
      			if(!success)
      			{
      				palloc_free_page (kpage);
      			}
      			else
      			{
      				int frame_index = ((vtop(kpage) & 0xfffff000)>>12);
		      		frames[frame_index].pid = thread_current()->tid;
		     			frames[frame_index].pte = (uint32_t*)((int)fault_addr & 0xfffff000);
		      		frames[frame_index].free_bit = false;
		      		frames[frame_index].kpage = kpage;
							struct sup_page_table_entry *spe = malloc(sizeof(struct sup_page_table_entry));;
			 	spe->va = (uint32_t*)((int)fault_addr & 0xfffff000);
			 	spe->type = 0;
			 	spe->file = NULL;
			 	spe->offset = 0;
			 	spe->page_read_bytes = 0;
			 	spe->page_zero_bytes = PGSIZE;
			 	spe->writable = true;
			 	list_push_back(&thread_current()->sup_page_table,&spe->elem);
			 	
			 	handled = true;
      			}
  		}
  		else
  		{
  			free_frame = find_free_frame();
  			if(free_frame != -1)
  			{
  				install_page ((void*)((int)fault_addr & 0xfffff000), frames[free_frame].kpage, true);
		      		struct sup_page_table_entry *spe = malloc(sizeof(struct sup_page_table_entry));
			 	spe->va = (uint32_t*)((int)fault_addr & 0xfffff000);
			 	spe->type = 0;
			 	spe->file = NULL;
			 	spe->offset = 0;
			 	spe->page_read_bytes = 0;
			 	spe->page_zero_bytes = PGSIZE;
			 	spe->writable = true;
			 	list_push_back(&thread_current()->sup_page_table,&spe->elem);
			 	
			 	handled = true;
			 	frames[free_frame].free_bit = false;
			 	frames[free_frame].pte = (uint32_t*)((int)fault_addr & 0xfffff000);
			 	frames[free_frame].pid = thread_current()->tid;
  			}
  			else
  			{
  				free_frame = evict_page();
  				install_page ((void*)((int)fault_addr & 0xfffff000), frames[free_frame].kpage, true);
		      		struct sup_page_table_entry *spe = malloc(sizeof(struct sup_page_table_entry));
			 	spe->va = (uint32_t*)((int)fault_addr & 0xfffff000);
			 	spe->type = 0;
			 	spe->file = NULL;
			 	spe->offset = 0;
			 	spe->page_read_bytes = 0;
			 	spe->page_zero_bytes = PGSIZE;
			 	spe->writable = true;
			 	list_push_back(&thread_current()->sup_page_table,&spe->elem);
			 	
			 	handled = true;
			 	frames[free_frame].free_bit = false;
			 	frames[free_frame].pte = (uint32_t*)((int)fault_addr & 0xfffff000);
			 	frames[free_frame].pid = thread_current()->tid;
  			}
  		}
  	}
  	else
  	{
  		f->eax=-1;
	  	thread_current()->parent->return_value=f->eax;
	  	printf("%s: exit(%d)\n",thread_name(),f->eax);
		thread_exit();
  	}
  }
  page_fault_cnt++;

}

int find_free_frame()
{
	int i;
	for(i=655;i<1024;i++)
	{
		if(frames[i].free_bit)
			return i;
	}
	return -1;
}
int evict_page()
{
	int i=clock_hand;
	while(pagedir_is_accessed((thread_from_pid(frames[i].pid))->pagedir,frames[i].pte))
	{
		pagedir_set_accessed((thread_from_pid(frames[i].pid))->pagedir,frames[i].pte,0);
		if(i==1023)
		{
			i=655;
		}
		else
			i=(i+1)%1024;
	}
	clock_hand = i;
	int j,k;
	for(j=0;j<1024;j++)
	{
		if(swap_slot_free[j])
			break;
	}
	for(k=0;k<8;k++)
	{
		disk_write(disk_get(1,1),k+j*8,frames[i].kpage+k*512);
	}
	swap_slot_free[j]=false;
	
	struct thread *evict = thread_from_pid(frames[i].pid);
	struct sup_page_table_entry *spe;
	struct list_elem *e;
	for (e = list_begin (&(evict->sup_page_table)); e != list_end (&(evict->sup_page_table)); e = list_next (e))
	{
		spe = list_entry (e, struct sup_page_table_entry, elem);
		if(spe->va == frames[i].pte)
		{
			spe->type = 3;
			spe->swap_slot_no = j;
			break;
		}
	}
	pagedir_clear_page(evict->pagedir,frames[i].pte);
	frames[i].free_bit = true;
	frames[i].pte = NULL;
	return i;
}
void frame_insert(int free_frame, struct sup_page_table_entry *spte)
{
	int j;
	switch(spte->type)
	{
		case 1:
			if(file_read(spte->file,frames[free_frame].kpage,spte->page_read_bytes) != (int) spte->page_read_bytes)
			{
				palloc_free_page (frames[free_frame].kpage);
				return;
			}
			memset(frames[free_frame].kpage+spte->page_read_bytes,0,spte->page_zero_bytes);
			if(!install_page(spte->va,frames[free_frame].kpage,spte->writable))
			{
				palloc_free_page (frames[free_frame].kpage);
				return;
			}
			break;
		case 2:
			while(frames[free_frame].kpage == NULL)
				frames[free_frame].kpage = palloc_get_page(PAL_USER);
			memset(frames[free_frame].kpage,0,spte->page_zero_bytes);
			if(!install_page(spte->va,frames[free_frame].kpage,spte->writable))
			{
				palloc_free_page(frames[free_frame].kpage);
				return;
			}
			break;
		case 3:
			for(j=0;j<8;j++)
			{
				disk_read(disk_get(1,1),j+spte->swap_slot_no*8,frames[free_frame].kpage+512*j);
			}
			if(!install_page(spte->va,frames[free_frame].kpage,spte->writable))
			{
				palloc_free_page (frames[free_frame].kpage);
				return;
			}
			swap_slot_free[spte->swap_slot_no] = true;
			break;
		default:
			printf("oops!!!!!Default....\n");
			break;
	}
	frames[free_frame].pid = thread_current()->tid;
	frames[free_frame].pte = spte->va;
	frames[free_frame].free_bit = false;
}
