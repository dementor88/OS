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
void _sys_exit (void);
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
void barrier_args(int *esp,int num);
void barrier_buf(char* buf, uint32_t size);
void barrier_str(char* str);
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
	if(!validate_address(esp)){
		//printf("first fault\n");
		_sys_exit();
		}
	int syscall_number = *esp++;

//	int *ptr=(int)pagedir_get_page(thread_current()->pagedir,*esp);
	
	if (syscall_number<SYS_HALT||syscall_number>SYS_INUMBER){
	//	printf("out of index\n");
		_sys_exit();
		return;
	}

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

void _sys_exit(void){
	struct thread *curr = thread_current();
	char *save_ptr;
	char *name = strtok_r(curr->name, " ", &save_ptr);
	printf("%s: exit(-1)\n",name);
	process_wake_parent (-1);
	thread_exit();
}
void sys_exit (uint32_t **esp){
	if(!validate_address((void *)*esp))
		_sys_exit();
	barrier_args((int *)*esp,1);
	int status = (int)*(*esp)++; 
	struct thread *curr = thread_current();
	char *save_ptr;
	char *name = strtok_r(curr->name, " ", &save_ptr);	//안그러면 쓸데없는 것까지 들어온다
	printf("%s: exit(%d)\n", name, status);  
	process_wake_parent (status);	// wake up waiting parent;
	thread_exit();
  
}

tid_t sys_exec (uint32_t **esp){
	barrier_args((int*)*esp,1);
	char *cmd_line = (char *)*(*esp)++;
	barrier_str(cmd_line);
	tid_t tid = process_execute(cmd_line);
	return tid;
}

int sys_wait (uint32_t **esp){
	barrier_args((int*)*esp,1);
	tid_t tid = (tid_t)*(*esp)++;
	int status = process_wait(tid); 
	return status;
}

bool sys_create (uint32_t **esp){
	barrier_args((int*)*esp,2);
	char *filename = (char *)*(*esp)++;
	uint32_t size = (uint32_t)*(*esp)++;
	barrier_str(filename);
	return filesys_create(filename,size);
}

bool sys_remove (uint32_t **esp){
	barrier_args((int*)*esp,1);
	char *file_name = (char *)*(*esp)++;
	barrier_str(file_name);
	return filesys_remove(file_name);
}

int  sys_open (uint32_t **esp){
	barrier_args((int*)*esp,1);
	char *filename = (char *)*(*esp)++; 
	struct file *f = filesys_open(filename);
	struct file_from_process *file_proc = malloc(sizeof(struct file_from_process));
	barrier_str(filename);
	file_proc->file = f;
	file_proc->fd = thread_current()->fd++;
	list_push_back(&thread_current()->file_list, &file_proc->elem);
	return file_proc->fd;	
}

int sys_filesize (uint32_t **esp){
	barrier_args((int*)*esp,1);
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
	barrier_args((int*)*esp,3);
	int fd = (int)*(*esp)++;
	char *buffer = (char *)*(*esp)++;
	uint32_t size = (uint32_t)*(*esp)++;
	barrier_buf(buffer,size);
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
	barrier_args((int*)*esp,3);
	int fd = (int)*(*esp)++;
	char *buffer = (char *)*(*esp)++;
	uint32_t size = (uint32_t)*(*esp)++;
	barrier_buf(buffer,size);
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
	barrier_args((int*)*esp,2);
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
	barrier_args((int*)*esp,1);
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
	barrier_args((int*)*esp,1);
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
	if(!is_user_vaddr (address) || address <(const void *)0x08084000){
		//printf("not valid addr\n");
		return false;
	}   
	return true;
}

void barrier_args(int *esp, int num){
	int i, *j;
	for (i = 0; i<num; i++){
		j = esp + i +1;
		if(!validate_address((void*)j))
			_sys_exit();
	}
}


void barrier_buf(char* buf, uint32_t size){
	if(buf == NULL || size <0)
		_sys_exit();
}
void barrier_str(char *str){
	if(str == NULL)
		_sys_exit();
	
}

