#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

/*
struct parent_child{
	struct thread *parent_thread;
	tid_t parent_tid;
	tid_t child_tid;
	bool waiting;
	int status;
	struct list_elem elem;
};
*/

static thread_func start_process NO_RETURN;
static bool load (char *cmdline, void (**eip) (void), void **esp);
//truct parent_child* getchild(struct thread* parent, tid_t tid);
//extern struct lock flock;

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (char *file_name) 
{
  char *fn_copy;
  tid_t tid;
  
  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  
  int size = strlen(file_name);
  char *backup = malloc(size*sizeof(char));
  
  int i =0;
  for(i=0; i<size; i++){
  	backup[i] = file_name[i];
  }
  backup[i]='\0';
  
  int c=0;
  for(i=0 ;i<size; i++){
  	if(file_name[i]==' ')
  		break;
  	c++;
  }
  
  char token[c + 1];
  for(i=0; i<c; i++)
  	token[i]=file_name[i];
  token[i]='\0';
    strlcpy(fn_copy,token,PGSIZE);
//	NOT_REACHED();
//	printf("%s\n",file_name);
//	NOT_REACHED();

  /* Create a new thread to execute FILE_NAME. */
  //tid = thread_create (file_name, PRI_DEFAULT, start_process, fn_copy);
  //tid = thread_create (file_name[1], PRI_DEFAULT, start_process, fn_copy);
  tid = thread_create (token, PRI_DEFAULT, start_process, backup);
  struct thread *child=get_child_from_tid(tid);
  if (tid == TID_ERROR)
    palloc_free_page (fn_copy); 
	
	//NOT_REACHED();
	//printf("%d\n",tid);*/
	
  return tid;
}

/* A thread function that loads a user process and makes it start
   running. */
static void
start_process (void *file_name_)
{
  sema_down(&(thread_current()->parent->parent_blocked));
  lock_acquire(&(thread_current()->parent->child_exec_lock));
  
  
  char *file_name = (char*)file_name_;
  struct intr_frame if_;
  bool success;
  
  
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_name, &if_.eip, &if_.esp);
  
   /*****************proj#2*/
   if(success){
		lock_release(&(thread_current()->parent->child_exec_lock));
   }
   char *token, *save_ptr;
   char *name_of_file;
   int c = 0;
   for (token = strtok_r(file_name, " ", &save_ptr); token != NULL; token = strtok_r (NULL, " ", &save_ptr)){
		if(c==0)
			name_of_file=token;
		c++;
   }
      
	char *fn_copy;
	fn_copy=palloc_get_page(0);
	strlcpy(fn_copy,name_of_file,PGSIZE);
	palloc_free_page (fn_copy);
  
	free(file_name_);
	if (!success) {
		thread_current()->parent->return_value=-1;
		lock_release(&(thread_current()->parent->child_exec_lock));
		thread_exit ();
	}

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid UNUSED) 
{
  //return -1;
  /*****************proj#2*/
  struct thread *child_thread=get_child_from_tid(child_tid);
  struct thread *current=thread_current();
  bool child_found=false;
  if(child_thread!=NULL){
  	struct list_elem *e;
  	for (e = list_begin(&(current->child_list)); e != list_end(&(current->child_list)); e = list_next(e)) {
		struct thread *f = list_entry (e, struct thread, child_list_elem);
		/*
			If the child is found then we block the current thread.
			Else it return -1
			Cases covered:
			1.if it is not the child of the calling process.
		*/
		if(f==child_thread){
			child_found=true;
			break;
		}
	}
	
	if(child_found){
		current->waiting_child=child_thread;
		child_thread->waited_on=true;
		
		thread_block();
		current->waiting_child=NULL;
		
		return current->return_value;
	}	
	else{
		return -1;
	}
  }else{
  	struct list_elem *e;
  	for (e = list_begin(&(current->dead_child_list)); e != list_end(&(current->dead_child_list)); e = list_next(e)){
		struct dead_child *f = list_entry (e, struct dead_child, elem);
		if(f->tid==child_tid){
			list_remove(e);
			return f->exit_status;
		}
	}
  	return -1;
  }
  /*
	struct parent_child *par_ch = getchild(thread_current(), child_tid);
	int status;
	if(par_ch!=NULL&&par_ch->parent_ptr == thread_current()){
		sema_down(&par_ch->exit);
		status = par_ch->status;
		list_remove(&par_ch->elem);
		free(par_ch);
		return status;
	}else
		return -1;
*/
//	struct parent_child *par_ch = getchild(thread_current(),child_tid);
}

/*************proj#2*/
/*
void
process_wake_parent (int status){
	tid_t tid = thread_tid();
	struct list_elem *e;
	for (e = list_begin (&parent_child_list); e != list_end (&parent_child_list); e = list_next(e)){		
		struct parent_child *par_ch = list_entry (e, struct parent_child, elem);
		if (par_ch->child_tid == tid){					
			par_ch->status = status;
			e = list_prev (list_remove (e));
			thread_unblock(par_ch->parent_ptr);		
			list_remove(&par_ch->elem);
		}
	}
}
*/

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;
/*	if(!lock_held_by_current_thread(&flock))
		lock_acquire(&flock);
	while(!list_empty(&thread_current()->file_list)){
		struct list_elem *e;
		struct files *fe;
		e = list_pop_front(&thread_current()->file_list);
		fe = list_entry(e, struct files, elem);
		file_close(fe->file);
		list_remove(&fe->elem);
		free(fe);
	}lock_release(&flock);*/
	
	pd = cur->pagedir;
	if (pd != NULL) {      
		cur->pagedir = NULL;
		pagedir_activate (NULL);
		pagedir_destroy (pd);
	}
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp,char *file_name);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
  /*
  	Intial Prototype --> bool
load (const char *file_name, void (**eip) (void), void **esp)
  */
bool
load (char *file_name, void (**eip) (void), void **esp) 
{

  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;
  
	/*****************pa2**********************/
	int size = strlen(file_name);
	char *backup_file = malloc(size*sizeof(char));
	int j;
	for(j=0; j<size; j++){
		backup_file[j]=file_name[j];
	}
	backup_file[j] = '\0';
	char *token, *save_ptr;
	char *f_name;
	int c  = 0;
	for (token = strtok_r(file_name, " ", &save_ptr); token != NULL; token = strtok_r(NULL, " ", &save_ptr)){
		if(c==0){
			f_name=token;
			c++;
		}
	
	}
	
	
  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();
  
 
  file = filesys_open (f_name);
  if (file == NULL) {
      printf ("load: %s: open failed\n", f_name);
      goto done; 
  }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", f_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp,backup_file))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;
 done:
  /* We arrive here whether the load is successful or not. */
  
  
  /*file_close (file);*/
  return success;
}

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;
	  
	  
	  /**********************************pa3**********************/
        if(page_read_bytes != 0){
         	struct sup_page_table_entry *spte = malloc(sizeof(struct sup_page_table_entry));
         	spte->va = upage;
         	spte->type = 1;
         	spte->file = file;
         	spte->offset = ofs;
			
         	spte->page_read_bytes = page_read_bytes;
         	spte->page_zero_bytes = page_zero_bytes;
         	spte->writable = writable;
         	list_push_back(&thread_current()->sup_page_table,&spte->elem);
        } else{
         	struct sup_page_table_entry *spte = malloc(sizeof(struct sup_page_table_entry));
         	spte->va = upage;
         	spte->type = 2;
         	spte->file = NULL;
         	spte->offset = 0;
			
         	spte->page_read_bytes = 0;
         	spte->page_zero_bytes = page_zero_bytes;
         	spte->writable = writable;
         	list_push_back(&thread_current()->sup_page_table,&spte->elem);
         }
		 
		 
	 /* Get a page of memory. 
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

       Load this page. 
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

       Add the page to the process's address space.
      if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }
		*/
	
      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
      ofs+=PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp,char *file_name) 
{
  uint8_t *kpage;
  bool success = false;
  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
      { 
      	*esp = PHYS_BASE -12;
      	/*************************pa3******************/
		int size=strlen(file_name);
		char *backup_file=malloc(size*sizeof(char));
		int j;
		for(j=0; j<size; j++){
			backup_file[j]=file_name[j];
		}
		backup_file[j]='\0';
		char *token, *save_ptr;
		char *file_n;
		int c = 0;
		
	   for (token = strtok_r (file_name, " ", &save_ptr); token != NULL; token = strtok_r (NULL, " ", &save_ptr)){
			if(c==0){
				file_n=token;
			}
			c++;
	   }
	   char *arg[c+1];
	   char *ptr;
	   j=0;
	   for (token = strtok_r (backup_file, " ", &ptr); token != NULL;token = strtok_r (NULL, " ", &ptr)){
			*esp=*esp-strlen(token)-1;
			strlcpy((char*)*esp,token,strlen(token)+1);
			arg[j]=(char*)*esp;
			j++;
	   }
	   *esp=*esp-1;
	   int mask=3;
	   
	   //Rounding Down code.
	   while(((int)(*esp)) & mask != 0){
			*esp = *esp-1;
			*(uint8_t *)*esp = 0;
	   }
	   arg[j]=NULL;
	   
	   *esp = *esp-4;
	   int temp = c;
	   char **addr_of_arg;
	   
	   while(temp>=0){
			**(char ***)esp=arg[temp];
			if(temp == 0){
				addr_of_arg=*esp;
			}
			temp--;
			*esp = *esp-4;
	   }
	   **(char ****)esp = addr_of_arg;
	   *esp = *esp-4;
	   *(int *)*esp = c;
	   *esp = *esp-4;
	   uint32_t p = 1;
	   **(uint32_t ***)esp=&p;
      } else {
        palloc_free_page (kpage);
      }
    }
  return success;
}
/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
   /*
   	Initial Prototype --> static bool
install_page (void *upage, void *kpage, bool writable);
   */
bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
