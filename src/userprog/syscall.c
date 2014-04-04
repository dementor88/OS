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

struct file_from_process{
	struct file *file;
	int fd;
	struct list_elem elem;
};

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
	uint32_t *esp = f->esp;
	if (!validate_address(esp)){
		//sys_exit_real (-1);
		thread_exit();
	}

	int syscall_number = *esp++;
	switch (syscall_number) {
		case SYS_HALT:	// Halt the operating system. 
		  power_off();	//threads/init.h
		  break;

		case SYS_EXIT:	// Terminate this process. 
		  sys_exit(&esp);
		  break;

		case SYS_EXEC:	// Start another process. 
		  f->eax = sys_exec(&esp);
		  break;

		case SYS_WAIT:	// Wait for a child process to die.
		  f->eax = sys_wait(&esp);
		  break;

		case SYS_CREATE:	// Create a file. 
		  f->eax = sys_create(&esp);
		  break;

		case SYS_REMOVE:	// Delete a file.
		  f->eax = sys_remove(&esp);
		  break;

		case SYS_OPEN:	// Open a file. 
		  f->eax = sys_open(&esp);
		  break;
		  
		case SYS_FILESIZE:	// Obtain a file's size. 
		  f->eax = sys_filesize(&esp);
		  break;
		  
		case SYS_READ:	// Read from a file. 
		  f->eax = sys_read(&esp);
		  break;

		case SYS_WRITE:
		  f->eax = sys_write(&esp);
		  break;

		case SYS_SEEK:	// Change position in a file. 
		  sys_seek(&esp);
		  break;

		case SYS_TELL: 	// Report current position in a file. 
		  f->eax = sys_tell(&esp);
		  break;
		  
		case SYS_CLOSE:	// Close a file.
		  sys_close(&esp);
		  break;

		default:
		  NOT_REACHED ();
		  break;
	}
}

void sys_exit (uint32_t **esp){
	int status = (int)*(*esp)++; 
	struct thread *curr = thread_current();
	char *save_ptr;
	char *name = strtok_r(curr->name, " ", &save_ptr);	//안그러면 쓸데없는 것까지 들어온다
	printf("%s: exit(%d)\n", name, status);  
	process_wake_parent (status);	// wake up waiting parent;
	thread_exit();
  
}

tid_t sys_exec (uint32_t **esp){
	char *cmd_line = (char *)*(*esp)++;
	tid_t tid = process_execute(cmd_line);
	return tid;
}

int sys_wait (uint32_t **esp){
	tid_t tid = (tid_t)*(*esp)++;
	int status = process_wait(tid); 
	return status;
}

bool sys_create (uint32_t **esp){
	char *filename = (char *)*(*esp)++;
	if (!validate_address (filename)){
		thread_exit();
	}
	uint32_t size = (uint32_t)*(*esp)++;
	return filesys_create(filename,size);
}

bool sys_remove (uint32_t **esp){
	char *file_name = (char *)*(*esp)++;
	return filesys_remove(file_name);
}

int  sys_open (uint32_t **esp){
	char *filename = (char *)*(*esp)++;   
	if (!validate_address (filename)){
		thread_exit();
	}
	struct file *f = filesys_open(filename);
	struct file_from_process *file_proc = malloc(sizeof(struct file_from_process));
	file_proc->file = f;
	file_proc->fd = thread_current()->fd++;
	list_push_back(&thread_current()->file_list, &file_proc->elem);
	return file_proc->fd;	
}

int sys_filesize (uint32_t **esp){
	int fd = (int)*(*esp)++;	
	struct thread *curr = thread_current();
	struct list_elem *e;
	for (e = list_begin(&curr->file_list); e != list_end(&curr->file_list); e = list_next(e)){
		struct file_from_process *file_proc = list_entry(e, struct file_from_process, elem);
		if(file_proc->fd == fd){
			int size = file_length(file_proc->file);
			return size; 
		}
	}	
	
}

int sys_read (uint32_t **esp){
	int fd = (int)*(*esp)++;
	char *buffer = (char *)*(*esp)++;
	uint32_t size = (uint32_t)*(*esp)++;
	
	
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
		struct thread *curr = thread_current();
		
		struct list_elem *e;
		for (e = list_begin(&curr->file_list); e != list_end(&curr->file_list); e = list_next(e)){
			struct file_from_process *file_proc = list_entry(e, struct file_from_process, elem);
			if(file_proc->fd == fd){
				int bytes = file_read(file_proc->file, buffer, size);
				return bytes;
			}
		}	
		
	}
	else
		return -1;	
}


int sys_write (uint32_t **esp){ 
	int fd = (int)*(*esp)++;
	char *buffer = (char *)*(*esp)++;
	uint32_t size = (uint32_t)*(*esp)++;

	//checking if invalid pointer access
	if (!validate_address (buffer)){
		thread_exit();
	}
	if (!validate_address (buffer)){
		//sys_exit_real (-1);
		thread_exit();
	}

	// write to console
	if ( fd == 1 ){
		putbuf(buffer, size);
		return size;
	}
	else if( fd >= 2){
		struct thread *curr = thread_current();

		
		
		struct list_elem *e;
		for (e = list_begin(&curr->file_list); e != list_end(&curr->file_list); e = list_next(e)){
			struct file_from_process *file_proc = list_entry(e, struct file_from_process, elem);
			if(file_proc->fd == fd){
				int bytes = file_write(file_proc->file, buffer, size);
				return bytes;
			}		
		}
	}
	else
		return -1;
}


void  sys_seek (uint32_t **esp){
	int fd = (int)*(*esp)++;
	uint32_t position = (uint32_t)*(*esp)++;
	struct thread *curr = thread_current();
	struct list_elem *e;
	for (e = list_begin(&curr->file_list); e != list_end(&curr->file_list); e = list_next(e)){
		struct file_from_process *file_proc = list_entry(e, struct file_from_process, elem);
		if(file_proc->fd == fd){
			file_seek(file_proc->file, position);
		}
	}	
}

unsigned sys_tell (uint32_t **esp){
	int fd = (int)*(*esp)++;
	struct thread *curr = thread_current();
	struct list_elem *e;
	for (e = list_begin(&curr->file_list); e != list_end(&curr->file_list); e = list_next(e)){
		struct file_from_process *file_proc = list_entry(e, struct file_from_process, elem);
		if(file_proc->fd == fd){
			off_t offset = file_tell(file_proc->file);
			return offset;
		}
	}
}

void sys_close (uint32_t **esp){
	int fd = (int)*(*esp)++;
	struct thread *curr = thread_current(); 
	struct list_elem *e;
	for (e = list_begin(&curr->file_list); e != list_end(&curr->file_list); e = list_next(e)){
		struct file_from_process *file_proc = list_entry(e, struct file_from_process, elem);
		if(file_proc->fd == fd){
			file_close(file_proc->file);
		}
	}
}

bool validate_address (void *address){
	if(is_user_vaddr (address) == 0){
		return false;
	}   
	return true;
}
