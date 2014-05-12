#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"


static void syscall_handler (struct intr_frame *);


/*****************proj#2*/
void exit ( struct intr_frame *f);
void exec ( struct intr_frame *f);
void wait (struct intr_frame *f);
void create (struct intr_frame *f);
void open (struct intr_frame *f);
void read (struct intr_frame *f);
void write (struct intr_frame *f);
void close(struct intr_frame *f);
void filesize (struct intr_frame *f);
void seek (struct intr_frame *f);
void tell (struct intr_frame *f);
void remove (struct intr_frame *f);



void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{  
	if(f->esp==NULL || lookup_page(thread_current()->pagedir,f->esp,false)== NULL){
		f->eax=-1;
		printf("%s: exit(%d)\n",thread_name(),f->eax);
		thread_exit();
		return;
	}
	if(!is_user_vaddr(((f->esp)+4))){
		f->eax=-1;
		printf("%s: exit(%d)\n",thread_name(),f->eax);
		thread_exit();
		return;
	}
	
	switch(*(int *)(f->esp)){
		case SYS_HALT:		// Halt the operating system. 
  			power_off();	//threads/init.h
  			break;
			
		case SYS_EXIT:		// Terminate this process. 
  			exit(f);  			
  			break;
			
		case SYS_EXEC:	// Start another process. 
  			exec(f);
			break;
			
		case SYS_WAIT:	// Wait for a child process to die.
			wait(f);			
  			break;
			
		case SYS_CREATE:	// Create a file. 
  			create(f);
  			break;
			
		case SYS_REMOVE:	// Delete a file.
  			remove(f);
  			break;
			
		case SYS_OPEN:	// Open a file. 
			open(f);
  			break;
			
		case SYS_FILESIZE:	// Obtain a file's size. 
			filesize(f);
  			break;
				
		case SYS_READ:	// Read from a file. 
  			read(f);
  			break;
			
		case SYS_WRITE:
  			write(f);
  			break;
			
		case SYS_SEEK:	// Change position in a file. 
  			seek(f);
  			break;
			
		case SYS_TELL:	// Report current position in a file. 
  			tell(f);
  			break;
			
		case SYS_CLOSE:	// Close a file.
  			close(f);
  			break;
			
  }  
}


void exit (struct intr_frame *f){
	void *vir_esp;
	int pid_exec;
	struct list_elem *e;
	vir_esp=((f->esp)+4);
	int status=*(int*)vir_esp;
	f->eax=*(int*)vir_esp;
	if(thread_current()->waited_on)
		thread_current()->parent->return_value=f->eax;
	else{
		thread_current()->present_in_dead_child_list = true;
		struct dead_child *dead=malloc(sizeof(struct dead_child));
		dead->tid=thread_current()->tid;
		dead->exit_status=f->eax;
		list_push_back(&thread_current()->parent->dead_child_list,&dead->elem);
	}
	printf("%s: exit(%d)\n",thread_name(),f->eax);
	thread_exit();
  
}

void exec ( struct intr_frame *f){
	void *vir_esp;
	int pid_exec;
	struct list_elem *e;
	vir_esp=(f->esp)+4;
	if(lookup_page(thread_current()->pagedir,*(const char **)vir_esp,false)!=NULL){
		pid_exec=process_execute(*(char**)vir_esp);
		list_push_back(&(thread_current()->child_exec_lock.semaphore.waiters),&(thread_current()->elem));
		thread_block();
		if(thread_current()->return_value==-1){
			f->eax=-1;
			return;
		}
		f->eax=pid_exec;
	}else{		
		f->eax=-1;
		thread_current()->parent->return_value=f->eax;
		printf("%s: exit(%d)\n",thread_name(),f->eax);
		thread_exit();
	}
}

void wait (struct intr_frame *f){
	void *vir_esp;
	int pid_exec;
	struct list_elem *e;
	thread_current()->wait_called=true;
  	f->eax=process_wait(*(int *)((f->esp)+4));
  	thread_current()->wait_called=false;
}

void create ( struct intr_frame *f){
	void *vir_esp;
	int pid_exec;
	struct list_elem *e;
	vir_esp=(f->esp)+4;
	if(lookup_page(thread_current()->pagedir,*(const char **)vir_esp,false)!=NULL){
		const char *name=*(const char**)vir_esp;
		if(name==NULL || strcmp(name,"")==0){
			f->eax=-1;
			thread_current()->parent->return_value=f->eax;
			printf("%s: exit(%d)\n",thread_name(),f->eax);
			thread_exit();
		}
		if(strlen(name) > 14){
			f->eax=0;
			thread_current()->parent->return_value=0;
			return;
		}
		int size=*(int*)(vir_esp+4);
		if(size	< 0){
			f->eax=-1;
			thread_current()->parent->return_value=f->eax;
			printf("%s: exit(%d)\n",thread_name(),f->eax);
			thread_exit();
		}
		f->eax=filesys_create(name,size);
	}else{
		f->eax=-1;
		thread_current()->parent->return_value=f->eax;
		printf("%s: exit(%d)\n",thread_name(),f->eax);
		thread_exit();
	}
			
}

void remove (struct intr_frame *f){
	void *vir_esp;
	int pid_exec;
	struct list_elem *e;
	if(lookup_page(thread_current()->pagedir,*(const char **)((f->esp)+4),false)!=NULL){
		if(strcmp(*(const char **)((f->esp)+4),"")==0){
			f->eax=0;
		}else{
			f->eax=filesys_remove(*(const char **)((f->esp)+4));
		}
	}else{
		f->eax=-1;
		thread_current()->parent->return_value=f->eax;
		printf("%s: exit(%d)\n",thread_name(),f->eax);
		return;
	}
}

void  open (struct intr_frame *f){
	void *vir_esp;
	int pid_exec;
	struct list_elem *e;
	vir_esp=(f->esp)+4;
	if(lookup_page(thread_current()->pagedir,*(const char **)vir_esp,false)!=NULL){
		const char *name=*(const char**)vir_esp;
		if(name==NULL || strcmp(name,"")==0){
			f->eax=-1;
			thread_current()->parent->return_value=f->eax;
			return;
		}
		struct file *file=filesys_open(name);
		if(file==NULL){
			f->eax=-1;
			thread_current()->parent->return_value=f->eax;
			return;
		}
		if(strcmp(name,thread_name())==0 || strcmp(name,thread_current()->parent->name)==0)	{
			file_deny_write(file);
		}
		struct open_file *o_f = malloc(sizeof(struct open_file));
		o_f->file=file;
		o_f->fd=thread_current()->last_fd+1;
		thread_current()->last_fd = o_f->fd;
		list_push_back(&thread_current()->open_files_list,&o_f->elem);
		f->eax=thread_current()->last_fd;
	}else{
		f->eax=-1;
		thread_current()->parent->return_value=f->eax;
		printf("%s: exit(%d)\n",thread_name(),f->eax);
		thread_exit();
	}
}

void filesize (struct intr_frame *f){
	void *vir_esp;
	int pid_exec;
	struct list_elem *e;
	for (e = list_begin(&thread_current()->open_files_list); e != list_end(&thread_current()->open_files_list);e = list_next(e)){
		struct open_file *o = list_entry (e, struct open_file, elem);
		if(o->fd==*(int*)((f->esp)+4)){
			f->eax=file_length(o->file);
			return;
		}
	}
	
}

void read (struct intr_frame *f){
	void *vir_esp;
	int pid_exec;
	struct list_elem *e;
	if((*(int*)((f->esp)+4)) == 0){
		input_getc();
		return;
	}
	for (e = list_begin(&thread_current()->open_files_list); e != list_end(&thread_current()->open_files_list);e = list_next(e)){
		struct open_file *o = list_entry (e, struct open_file, elem);
		if((o->fd)==(*(int*)((f->esp)+4)))
		{
			if(lookup_page(thread_current()->pagedir,*(char**)((f->esp)+8),false)!=NULL){
				f->eax=file_read(o->file,*(char**)((f->esp)+8),*(uint32_t*)(((f->esp)+12)));
				return;
			}else{
				f->eax=-1;
				thread_current()->parent->return_value=f->eax;
				printf("%s: exit(%d)\n",thread_name(),f->eax);
				thread_exit();
			}
		}
	}
		
	f->eax=-1;
}


void write (struct intr_frame *f){
	void *vir_esp;
	int pid_exec;
	struct list_elem *e;
	vir_esp=(f->esp)+4;
	int file_descriptor=*(int*)((f->esp)+4);
	int size;
	vir_esp=vir_esp+4;
	if(lookup_page(thread_current()->pagedir,*(const char **)vir_esp,false)!=NULL){
		const char *buffer=*(const char**)vir_esp;
		size=*(int*)(vir_esp+4);
		if(file_descriptor==1){
			putbuf(buffer,size);
		}else{
			for (e = list_begin (&thread_current()->open_files_list); e != list_end (&thread_current()->open_files_list);e = list_next (e)){
				struct open_file *o = list_entry (e, struct open_file, elem);
				if((o->fd)==(*(int*)((f->esp)+4))){
					if(lookup_page(thread_current()->pagedir,*(char**)((f->esp)+8),false)!=NULL){
						f->eax=file_write(o->file,*(char**)((f->esp)+8),*(uint32_t*)(((f->esp)+12)));
						return;
					}else{
						f->eax=-1;
						thread_current()->parent->return_value=f->eax;
						printf("%s: exit(%d)\n",thread_name(),f->eax);
						thread_exit();
					}
				}
			}
		}
		
	}else{
		f->eax=-1;
		thread_current()->parent->return_value=f->eax;
		printf("%s: exit(%d)\n",thread_name(),f->eax);
		thread_exit();
	}
}


void  seek (struct intr_frame *f){
	void *vir_esp;
	int pid_exec;
	struct list_elem *e;
	for (e = list_begin (&thread_current()->open_files_list); e != list_end (&thread_current()->open_files_list);e = list_next (e)) {
		struct open_file *o = list_entry (e, struct open_file, elem);
		if(o->fd==*(int*)((f->esp)+4)){
			if((*(int*)((f->esp)+8))>=0){
				file_seek(o->file,*(int*)((f->esp)+8));
				return;
			}
		}
	}	
}

void tell (struct intr_frame *f){
	void *vir_esp;
	int pid_exec;
	struct list_elem *e;
	for (e = list_begin (&thread_current()->open_files_list); e != list_end (&thread_current()->open_files_list);e = list_next (e)){
		struct open_file *o = list_entry (e, struct open_file, elem);
		if(o->fd==*(int*)((f->esp)+4)){
			f->eax=file_tell(o->file);
			return;
		}
	}
}

void close (struct intr_frame *f){
	void *vir_esp;
	int pid_exec;
	struct list_elem *e;
	for (e = list_begin (&thread_current()->open_files_list); e != list_end (&thread_current()->open_files_list);e = list_next (e)){
		struct open_file *o = list_entry (e, struct open_file, elem);
		if(o->fd==*(int*)((f->esp)+4))
		{
			list_remove(e);
			file_close(o->file);
		free(o);
			return;
		}
	}

}

