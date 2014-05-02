#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include <string.h>
#include "threads/init.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/input.h"

static void syscall_handler (struct intr_frame *);

/*****************proj#2*/
void _sys_exit (void);
void exit ( struct intr_frame *f);
tid_t exec ( struct intr_frame *f);
int wait (struct intr_frame *f);
bool create (struct intr_frame *f);
int open (struct intr_frame *f);
int read (struct intr_frame *f);
int write (struct intr_frame *f);
void close(struct intr_frame *f);
int filesize (struct intr_frame *f);
void seek (struct intr_frame *f);
unsigned tell (struct intr_frame *f);
bool remove (struct intr_frame *f);
bool validate_address (void *address);
void barrier_args(struct intr_frame *f,int num);
void barrier_buf(struct intr_frame *f);
void barrier_str(char* str);
struct file_from_process{
	struct file *file;
	int fd;
	struct list_elem elem;
};

void* kernel_addr(const void* fesp){
	uint32_t* pd = thread_current()->pagedir;
	if(fesp==NULL || pd == NULL){
	//	printf("non\n");
		_sys_exit();
		}
	if(fesp<(void*)0x08084000|| fesp>=PHYS_BASE/*!validate_address(fesp)*/){
		_sys_exit();
		}
	void * ret = pagedir_get_page(pd ,fesp);
	if(ret == NULL){
//		printf("non\n");
		_sys_exit();
		}
	return ret;
}
void* check_arg(struct intr_frame *f, int num){
	if(!validate_address(f->esp))
		_sys_exit();
	return (uint32_t *)f->esp+num+1;
}
void* get_arg(struct intr_frame *f, int num){
			
	return (void*)*((uint32_t*)kernel_addr((uint32_t*)f->esp+num+1));
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
    	if(!validate_address(f->esp)){
			_sys_exit();
		}
	void *esp = (void*)pagedir_get_page(thread_current()->pagedir,f->esp);//f->esp;
//	printf("fesp:%d, esp:%d",esp, f->esp);
	if(esp==NULL)
		_sys_exit();
/*
	if(!validate_address(f->esp)){
//		printf("first fault\n");
//		NOT_REACHED();
		_sys_exit();
		}
*/
//	uint32_t *esp = 
	
	uint32_t syscall_number =*(uint32_t*)esp;//*((int *)kernel_addr(f->esp));//*esp++;
//	printf("syscall num: %d\n",syscall_number);
//	int *ptr=(int)pagedir_get_page(thread_current()->pagedir,f->esp);
//	printf("sys:%d, ptr:%d",syscall_number,*ptr);	
	if (syscall_number<SYS_HALT||syscall_number>SYS_INUMBER){
//		printf("out of index\n");
		_sys_exit();
		return;
	}
//printf("%d\n",syscall_number);
	switch (syscall_number) {
		case SYS_HALT:	// Halt the operating system. 
		  power_off();	//threads/init.h
		  break;

		case SYS_EXIT:	// Terminate this process. 
		  exit(f);
		  break;

		case SYS_EXEC:	// Start another process. 
		  f->eax = exec(f);
		  break;

		case SYS_WAIT:	// Wait for a child process to die.
		  f->eax = wait(f);
		  break;

		case SYS_CREATE:	// Create a file. 
		  f->eax = create(f);
		  break;

		case SYS_REMOVE:	// Delete a file.
		  f->eax = remove(f);
		  break;

		case SYS_OPEN:	// Open a file. 
		  f->eax = open(f);
		  break;
		  
		case SYS_FILESIZE:	// Obtain a file's size. 
		  f->eax = filesize(f);
		  break;
		  
		case SYS_READ:	// Read from a file. 
		  f->eax = read(f);
		  break;

		case SYS_WRITE:
		  f->eax = write(f);
		  break;

		case SYS_SEEK:	// Change position in a file. 
		  seek(f);
		  break;

		case SYS_TELL: 	// Report current position in a file. 
		  f->eax = tell(f);
		  break;
		  
		case SYS_CLOSE:	// Close a file.
		  close(f);
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
//	curr->status = -1;
	process_wake_parent (-1);
	curr->exit = -1;
	thread_exit();
}
void exit (struct intr_frame *f){
//	if(!validate_address((void *)*esp))
//		_sys_exit();

	barrier_args(f,1);
	int status = (int)get_arg(f,0);//*((int*)kernel_addr(*esp++));//(int)*(*esp)++; 
	struct thread *curr = thread_current();
	char *save_ptr;
	char *name = strtok_r(curr->name, " ", &save_ptr);	//안그러면 쓸데없는 것까지 들어온다
	printf("%s: exit(%d)\n", name, status);  
	process_wake_parent (status);	// wake up waiting parent;
	curr->exit = status;
	thread_exit();
  
}

tid_t exec ( struct intr_frame *f){
	barrier_args(f,1);
	char *cmd_line = (char*)get_arg(f,0);//*((char*)kernel_addr(*esp++));//(char *)*(*esp)++;
	barrier_str(cmd_line);
/*DEBUG*/
//if(f->esp+1 == NULL)
//	_sys_exit();
//if(cmd_line == 0x20101234)
//	_sys_exit();
/*
	struct file *fl = filesys_open(cmd_line);
	if(fl==NULL)
		return -1;
	struct files *e = (struct files*)malloc(sizeof(struct files));
	e->fd = thread_current()->assignfd;
	thread_current()->assignfd+=1;
	e->file = fl;
	list_push_front(&thread_current()->file_list,&e->elem);
	return e->fd;
*/
//	printf("%s\n",cmd_line);
//	NOT_REACHED();
	struct file *exec_f;
	char *p;
	p = strchr(cmd_line,(int)(' '));
	if(p!=NULL){
		char file_name[p-cmd_line];
		strlcpy(file_name,cmd_line,(p-cmd_line+1));
		exec_f = filesys_open(file_name);
	}else
		exec_f = filesys_open(cmd_line);
	if(exec_f == NULL){
		if(p!=NULL){
			char file_name2[p-cmd_line];
			strlcpy(file_name2,cmd_line,(p-cmd_line+1));
			printf("load: %s: open failed\n",file_name2);
		}else
			printf("load: %s: open failed\n",cmd_line);
			printf("%s: exit(%d)\n", cmd_line, -1);
			return -1;
	}else{
		tid_t tid = process_execute(cmd_line);
		file_close(exec_f);
		return tid;
	}
//	tid_t tid = process_execute(cmd_line);
/*	struct list_elem *e;
	for(e = list_begin(&parent_child_list);e!=list_end(&parent_child_list);
			e = list_next(e)){
		struct parent_child *par_ch = list_entry(e, struct parent_child,elem);
		if(par_ch->child_tid == tid){

		}
	}*/
//	printf("%s, %d\n",cmd_line,tid);
//NOT_REACHED();
	return -1;
}

int wait (struct intr_frame *f){
	barrier_args(f,1);
	tid_t tid = (tid_t)get_arg(f,0);//*((tid_t*)kernel_addr(*esp++));//(tid_t)*(*esp)++;
	int status = process_wait(tid); 
//	if(status == -1)
//		_sys_exit();
	return status;
}

bool create ( struct intr_frame *f){
	barrier_args(f,2);
	char *filename = (char*)get_arg(f,0);//*((char*)kernel_addr(*esp++));//(char *)*(*esp)++;
	uint32_t size = (uint32_t)get_arg(f,1);//(uint32_t)*(*esp)++;
	barrier_str(filename);
/*DEBUG*/
//	if(filename == 0x20101234)
//		_sys_exit();

	return filesys_create(filename,size);
}

bool remove (struct intr_frame *f){
	barrier_args(f,1);
	char *file_name = (char*)get_arg(f,0);//(char *)*(*esp)++;
	barrier_str(file_name);
	return filesys_remove(file_name);
}

int  open (struct intr_frame *f){
	barrier_args(f,1);
	char *filename = (char*)get_arg(f,0);//(char *)*(*esp)++; 
	barrier_str(filename);
/*DEBUG*/
//if(filename == 0x20101234)
//	_sys_exit();
/*
	struct file *fl = filesys_open(filename);
	if(fl==NULL)
		return -1;
	struct files *e = (struct files*)malloc(sizeof(struct files));
	e->fd = thread_current()->assignfd;
	thread_current()->assignfd+=1;
	e->file = fl;
	list_push_front(&thread_current()->file_list,&e->elem);
	return e->fd;
*/
	lock_acquire(&flock);
	struct file *fl = filesys_open(filename);
	if(fl ==NULL)
		return -1;
	struct file_from_process *file_proc = malloc(sizeof(struct file_from_process));
	file_proc->file = fl;
	file_proc->fd = thread_current()->fd++;
	list_push_back(&thread_current()->file_list, &file_proc->elem);
	lock_release(&flock);
	return file_proc->fd;	
}

int filesize (struct intr_frame *f){
	barrier_args(f,1);
	int fd = (int)get_arg(f,0);//(int)*(*esp)++;	
	struct thread *curr = thread_current();
	struct list_elem *e;
	lock_acquire(&flock);
	for (e = list_begin(&curr->file_list); e != list_end(&curr->file_list); e = list_next(e)){
		struct file_from_process *file_proc = list_entry(e, struct file_from_process, elem);
		if(file_proc->fd == fd){
			int size = file_length(file_proc->file);
			lock_release(&flock);
			return size; 
		}
	}lock_release(&flock);	
	
}

int read (struct intr_frame *f){
	barrier_args(f,3);
	barrier_buf(f);
	int fd = (int)get_arg(f,0);//(int)*(*esp)++;
	char *buffer = (char*)get_arg(f,1);//(char *)*(*esp)++;
	uint32_t size = (uint32_t)get_arg(f,2);//(uint32_t)*(*esp)++;
//	barrier_buf(buffer,size);
/*DEBUG*/
//if(f->esp+1 ==NULL)
//	_sys_exit();
//if(buffer==0xc0100000)
//	_sys_exit();

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
		lock_acquire(&flock);
		struct thread *curr = thread_current();
		struct list_elem *e;
		for (e = list_begin(&curr->file_list); e != list_end(&curr->file_list); e = list_next(e)){
			struct file_from_process *file_proc = list_entry(e, struct file_from_process, elem);
			if(file_proc->fd == fd){
				int bytes = file_read(file_proc->file, buffer, size);
				lock_release(&flock);
				return bytes;
			}
		}	
		lock_release(&flock);	
	}
	else
		return -1;	
}


int write (struct intr_frame *f){
	barrier_args(f,3);
	barrier_buf(f);
//printf("write");
	int fd = (int)get_arg(f,0);//(int)*(*esp)++;
//	printf("orign:%d,  new:%d",(int)*(*esp)++,fd);
	char *buffer = (char *)get_arg(f,1);//(char *)*(*esp)++;
	uint32_t size = (uint32_t)get_arg(f,2); //(uint32_t)*(*esp)++;
//printf("\tfd:%d, buf:%s, size:%d\n",fd, buffer, size);
//	barrier_buf(buffer,size);
/*DEBUG*/
//if(buffer==NULL)
//	_sys_exit();
//if(buffer==0x10123420)
//	_sys_exit();
	// write to console
	if ( fd == 1 ){
		putbuf(buffer, size);
		return size;
	}
	else if( fd >= 2){
		lock_acquire(&flock);
		struct thread *curr = thread_current();
		struct list_elem *e;
		for (e = list_begin(&curr->file_list); e != list_end(&curr->file_list); e = list_next(e)){
			struct file_from_process *file_proc = list_entry(e, struct file_from_process, elem);
			if(file_proc->fd == fd){
				int bytes = file_write(file_proc->file, buffer, size);
		lock_release(&flock);
				return bytes;
			}		
		}
		lock_release(&flock);
	}
	else
		return -1;
}


void  seek (struct intr_frame *f){
	barrier_args(f,2);
	int fd = (int)get_arg(f,0);//(int)*(*esp)++;
	uint32_t position = (uint32_t)get_arg(f,1);//(uint32_t)*(*esp)++;
	struct thread *curr = thread_current();
	struct list_elem *e;
	lock_acquire(&flock);
	for (e = list_begin(&curr->file_list); e != list_end(&curr->file_list); e = list_next(e)){
		struct file_from_process *file_proc = list_entry(e, struct file_from_process, elem);
		if(file_proc->fd == fd){
			file_seek(file_proc->file, position);
		}
	}lock_release(&flock);	
}

unsigned tell (struct intr_frame *f){
	barrier_args(f,1);
	int fd = (int)get_arg(f,0);//(int)*(*esp)++;
	struct thread *curr = thread_current();
	struct list_elem *e;
	lock_acquire(&flock);
	for (e = list_begin(&curr->file_list); e != list_end(&curr->file_list); e = list_next(e)){
		struct file_from_process *file_proc = list_entry(e, struct file_from_process, elem);
		if(file_proc->fd == fd){
			off_t offset = file_tell(file_proc->file);
			lock_release(&flock);
			return offset;
		}
	}lock_release(&flock);
}

void close (struct intr_frame *f){
	barrier_args(f,1);
	int fd = (int)get_arg(f,0);//(int)*(*esp)++;
	struct thread *curr = thread_current(); 
	struct list_elem *e;
	lock_acquire(&flock);
	for (e = list_begin(&curr->file_list); e != list_end(&curr->file_list); e = list_next(e)){
		struct file_from_process *file_proc = list_entry(e, struct file_from_process, elem);
		if(file_proc->fd == fd){
			file_close(file_proc->file);
			list_remove(e);
		}
	}lock_release(&flock);

}

bool validate_address (void *address){
	if(!is_user_vaddr (address) || address <(const void *)0x08084000){
		//printf("not valid addr\n");
		return false;
	}   
	return true;
}
/*
struct parent_child* child(tid_t tid){
	struct list_elem *e;
	for (e = list_begin(&thread_current()->child); e!=list_end(&thread_current()->child);
			e = list_next(e)){
		struct parent_child *pc = list_entry(e,struct parent_child, elem);
		if(tid == pc->parent_tid)
			return pc;
	}
	return NULL;
}
*/

void barrier_args(struct intr_frame *f, int num){
	int i, *j;
	for (i = 0; i<num; i++){
		j = (int*)f->esp + i;
		if(!validate_address((void*)j))
			_sys_exit();
	}
}


void barrier_buf(struct intr_frame *f){
	if(f == NULL )
		_sys_exit();
	int i;
	int size = (int)get_arg(f,2);
	char* ptr = (char*)get_arg(f,1);
	if(ptr==NULL)
		_sys_exit();
	if(UNMAP(ptr))
		_sys_exit();
/*	
	printf("%s\n",(char*)get_arg(f,1));
	for(i =0;i<size;i++){
		printf("  %c", ((char*)(get_arg(f,1))+i));
		if(!validate_address(ptr))
			_sys_exit();
		ptr++;
	}
	printf("\n");
	*/
}
void barrier_str(char *str){
	if(str == NULL)
		_sys_exit();
	if(UNMAPPED(str))
		_sys_exit();
//	if(str+1 ==NULL)
//		_sys_exit();
//	if(!validate_address(str))
//		_sys_exit();
//	while(*(char*)kernel_addr(str)!=0)
//		str= (char*)str +1;
	
}

void page_bounary(int *esp){
	
	if(pagedir_get_page(thread_current()->pagedir, (const void*) esp)== NULL)
		_sys_exit();

}


