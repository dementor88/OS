#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "threads/init.h"
#include "threads/vaddr.h"

#define		ARG0(INTR_FRAME)	(INTR_FRAME->esp)
#define		ARG1(INTR_FRAME)	((INTR_FRAME->esp)+4)
#define		ARG2(INTR_FRAME)	((INTR_FRAME->esp)+8)
#define		ARG3(INTR_FRAME)	((INTR_FRAME->esp)+12)

static void syscall_handler (struct intr_frame *);
static inline bool isValidPtr (uint32_t *pagedir, const void* ptr);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static inline bool isValidPtr (uint32_t *pagedir, const void* ptr)
{
	//printf("%x is_user_vaddr? %d\n",(unsigned)(ptr), is_user_vaddr(ptr));
	//printf("pagedir_get_page != NULL? %d\n", pagedir_get_page(pagedir,ptr) != NULL);
	return is_user_vaddr(ptr) && ((unsigned)pagedir_get_page(pagedir, ptr) != (unsigned)NULL);
}

static void
syscall_handler (struct intr_frame *f) 
{
	int num;
	uint32_t *pagedir = thread_current()->pagedir;
	if(!isValidPtr(pagedir, ARG0(f)))
		thread_exit(-1);

	num = *(int*)ARG0(f);

	switch(num)
	{
		/* SYSCALL3 */
		case SYS_READ:
		case SYS_WRITE:
			if(!isValidPtr(pagedir, ARG3(f)))
				thread_exit(-1);
		/* SYSCALL2 */
		case SYS_CREATE: 
		case SYS_SEEK:
			if(!isValidPtr(pagedir, ARG2(f)))
				thread_exit(-1);
		/* SYSCALL1 */
		case SYS_EXIT:
		case SYS_EXEC:
		case SYS_WAIT:
		case SYS_REMOVE:
		case SYS_OPEN:
		case SYS_FILESIZE:
		case SYS_TELL:
		case SYS_CLOSE:
			if(!isValidPtr(pagedir, ARG1(f)))
				thread_exit(-1);
		/* SYSCALL0 */
		case SYS_HALT:
			// Nothing to do
			break;
	}
	switch(num)
	{
		case SYS_HALT:
			__halt();
			break;
		case SYS_EXIT:
			__exit(*(int*)ARG1(f));	
			break;
		case SYS_EXEC:
			if(!isValidPtr(pagedir, *(const char**)ARG1(f)))
				thread_exit(-1);
			f->eax = (pid_t)__exec(*(const char**)ARG1(f));
			break;
		case SYS_WAIT:
			f->eax = (int)__wait(*(pid_t*)ARG1(f));
			break;
		case SYS_CREATE:
			if(!isValidPtr(pagedir, *(const char**)ARG1(f)))
				thread_exit(-1);
			f->eax = (bool)__create(*(const char**)ARG1(f), *(unsigned*)ARG2(f));
			break;
		case SYS_REMOVE:
			if(!isValidPtr(pagedir, *(const char**)ARG1(f)))
				thread_exit(-1);
			f->eax = (bool)__remove(*(const char**)ARG1(f));
			break;
		case SYS_OPEN:
			if(!isValidPtr(pagedir, *(const char**)ARG1(f)))
				thread_exit(-1);
			f->eax = (int)__open(*(const char**)ARG1(f));
			break;
		case SYS_FILESIZE:
			f->eax = (int)__filesize(*(int*)ARG1(f));
			break;
		case SYS_READ:
			if(!isValidPtr(pagedir, *(const char**)ARG2(f)))
				thread_exit(-1);
			f->eax = (int)__read(*(int*)ARG1(f), *(void**)ARG2(f), *(unsigned*)ARG3(f));
			break;
		case SYS_WRITE:
			if(!isValidPtr(pagedir, *(const char**)ARG2(f)))
				thread_exit(-1);
			f->eax = (int)__write(*(int*)ARG1(f), *(const void**)ARG2(f), *(unsigned*)ARG3(f));
			break;
		case SYS_SEEK:
			__seek(*(int*)ARG1(f), *(unsigned*)ARG2(f));
			break;
		case SYS_TELL:
			f->eax = (unsigned)__tell(*(int*)ARG1(f));
			break;
		case SYS_CLOSE:
			__close(*(int*)ARG1(f));
			break;
		default:
			thread_exit(-1);
	}
}

static void
__halt ()
{
	power_off();
}

static void
__exit (int status)
{
	thread_exit(status);
}

static pid_t
__exec (const char *file)
{
	return (pid_t)process_execute(file);
}

static int
__wait (pid_t pid)
{
	return (int)process_wait(pid);
}

static bool
__create (const char *file, unsigned initial_size)
{
	if(file == NULL)
		thread_exit(-1);
	return filesys_create (file, initial_size);
}

static bool
__remove (const char *file)
{
	if(file == NULL)
		thread_exit(-1);
	return filesys_remove (file);
}

static int
__open (const char *file)
{
	//file_open();
	if(file == NULL)
		thread_exit(-1);
	return fd_open(file);
}

static int
__filesize (int fd)
{
	return fd_filesize(fd);
}

static int
__read (int fd, void *buffer, unsigned length)
{
	//file_read();
	int size;
	if(fd == STDIN_FILENO)
	{
		for(size =0; size<(int)length; size++)
			*((uint8_t*)buffer++) = input_getc();
	}
	if(fd == STDOUT_FILENO)
		return -1;

	return fd_read(fd, buffer, length);
}

static int
__write (int fd, const void* buffer, unsigned length)
{
	//file_write();
	if(fd == STDOUT_FILENO)
	{
		putbuf((const char*)buffer, length);
		return length;
	}
	if(fd == STDIN_FILENO)
		return -1;
	return fd_write(fd, buffer, length);
}

static void
__seek (int fd, unsigned position)
{
	fd_seek(fd, position);
}

static unsigned
__tell (int fd)
{
	return fd_tell(fd);
}

static void
__close (int fd)
{
	//file_close();
	fd_close(fd);
}
