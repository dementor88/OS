#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <string.h>
#include "threads/init.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/input.h"

static void syscall_handler (struct intr_frame *);
/*****************proj#2*/
void sys_exit (uint32_t **esp);
void sys_exit_real (int status);
tid_t sys_exec (uint32_t **esp);
int sys_wait (uint32_t **esp);
bool sys_create (uint32_t **esp);
int sys_open (uint32_t **esp);
int sys_read (uint32_t **esp);
int sys_write (uint32_t **esp);
void sys_close(uint32_t **esp);
int sys_filesize (uint32_t **esp);
void sys_seek (uint32_t **esp);
unsigned sys_tell (uint32_t **esp);
bool sys_remove (uint32_t **esp);
bool validate_address (void *address);
/*****************proj#2*/
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}
/*****************proj#2
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");
  thread_exit ();
}
*/
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
	uint32_t *esp = f->esp;
	if (!validate_address(esp)){
		sys_exit_real (-1);
	}

	int syscall_number = *esp++;
	switch (syscall_number) {
		case SYS_HALT:                   /* Halt the operating system. */
		  power_off();
		  break;

		case SYS_EXIT:                   /* Terminate this process. */
		  sys_exit(&esp);
		  break;

		case SYS_EXEC:                   /* Start another process. */
		  f->eax = sys_exec(&esp);
		  break;

		case SYS_WAIT:                   /* Wait for a child process to die. */
		  f->eax = sys_wait(&esp);
		  break;

		case SYS_CREATE:                 /* Create a file. */
		  f->eax = sys_create(&esp);
		  break;

		case SYS_REMOVE:                 /* Delete a file. */
		  f->eax = sys_remove(&esp);
		  break;

		case SYS_OPEN:                   /* Open a file. */
		  f->eax = sys_open(&esp);
		  break;
		  
		case SYS_FILESIZE:               /* Obtain a file's size. */
		  f->eax = sys_filesize(&esp);
		  break;
		  
		case SYS_READ:                   /* Read from a file. */
		  f->eax = sys_read(&esp);
		  break;

		case SYS_WRITE:
		  f->eax = sys_write(&esp);
		  break;

		case SYS_SEEK:                   /* Change position in a file. */
		  sys_seek(&esp);
		  break;

		case SYS_TELL:                   /* Report current position in a file. */
		  f->eax = sys_tell(&esp);
		  break;
		  
		case SYS_CLOSE:                  /* Close a file. */
		  sys_close(&esp);
		  break;

		default:
		  NOT_REACHED ();
		  break;
	}
}

void
sys_exit (uint32_t **esp){
  int status = (int)*(*esp)++; 
  sys_exit_real (status);
}

void
sys_exit_real (int status){
  struct thread *cur = thread_current();
  char *save_ptr;
  char *name = strtok_r(cur->name, " ", &save_ptr);
  printf("%s: exit(%d)\n", name, status);  
  // wake up waiting parent;
  process_awake_waiter (status);

  thread_exit();
}

tid_t
sys_exec (uint32_t **esp){
  char *cmd_line = (char *)*(*esp)++;
  // temporary implementation
  // should implement in process.c
  tid_t tid = process_execute(cmd_line);
  return tid;
}

int
sys_wait (uint32_t **esp){
  tid_t tid = (tid_t)*(*esp)++;

  int status = process_wait(tid);
  return status;
}

bool
sys_create (uint32_t **esp){
	char *filename = (char *)*(*esp)++;
	uint32_t size = (uint32_t)*(*esp)++;

	return filesys_create(filename,size);
}

int 
sys_open (uint32_t **esp){
	char *filename = (char *)*(*esp)++;    
	//checking if invalid pointer access
	if (!validate_address (filename)) {
		sys_exit_real (-1);
	}
	struct thread *t = thread_current();
	int i;
	for(i=2;i<128;i++){
		if(t->fdtable[i]==NULL){
			t->fdtable[i] = filesys_open(filename);
			if(filename == NULL || t->fdtable[i] == NULL) return -1;
			else return i;
		}
	}
	return -1;
}

int sys_read (uint32_t **esp){
	int fd = (int)*(*esp)++;
	char *buffer = (char *)*(*esp)++;
	uint32_t size = (uint32_t)*(*esp)++;

	//checking if invalid pointer access
	if (!validate_address (buffer)){
		sys_exit_real (-1);
	}
	// read from console
	if( fd == 0 ){
		uint32_t cnt = 0;
		while (1) {
			uint8_t key = input_getc();
			if(key==13){			// when input key == carrage return  
				buffer[cnt] = '\0';		//exit
				break;
			}
			else buffer[cnt++] = key;
			if (cnt >= size)
				break;
		}
		return size;
	}
	// read from user file
	else if( fd >= 2 ){
		struct thread *t = thread_current();
		if(t->fdtable[fd] == NULL ) return -1;
		else return file_read(t->fdtable[fd],buffer,size);
	}
	else
		return -1;
}


int
sys_write (uint32_t **esp){ 
	int fd = (int)*(*esp)++;
	char *buffer = (char *)*(*esp)++;
	uint32_t size = (uint32_t)*(*esp)++;

  //checking if invalid pointer access
  if (!validate_address (buffer)){
    sys_exit_real (-1);
  }

  // write to console
  if ( fd == 1 ){
    putbuf(buffer, size);
    return size;
  }
  else if( fd >= 2){
    struct thread *t = thread_current();

    if (t->fdtable[fd] == NULL ) 
		return -1;
    else 
		return file_write(t->fdtable[fd],buffer,size);
  }
  else
    return -1;
}

void 
sys_close (uint32_t **esp){
	int fd = (int)*(*esp)++;
	struct thread *t = thread_current();
	file_close(t->fdtable[fd]);
	t->fdtable[fd] = NULL;
}

int
sys_filesize (uint32_t **esp){
	int fd = (int)*(*esp)++;
	struct thread *t = thread_current();
	if(t->fdtable[fd] == NULL) return -1;
	else return (file_length(t->fdtable[fd]));
}

void 
sys_seek (uint32_t **esp){
	int fd = (int)*(*esp)++;
	struct thread *t = thread_current();
	uint32_t position = (uint32_t)*(*esp)++;

	file_seek(t->fdtable[fd],position);
}

unsigned
sys_tell (uint32_t **esp){
	int fd = (int)*(*esp)++;
	struct thread *t = thread_current();
	return (file_tell(t->fdtable[fd]));
}

bool
sys_remove (uint32_t **esp){
	char *file_name = (char *)*(*esp)++;
	return filesys_remove(file_name);
}

bool
validate_address (void *address){
	struct thread *cur = thread_current();
	uint32_t *pd = cur->pagedir;  

	if(is_user_vaddr (address) == 0 || pagedir_get_page (pd, address) == NULL){
		return false;
	}   
	return true;
}
