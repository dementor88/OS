#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#define UNMAPPED(i) (i)==0x10123420 ||(i)==0x20101234
#define UNMAP(i) (i)==0xc0100000 ||(i)==0x10123420

#define USER_VADDR_BOTTOM ((void *) 0x08048000)
#define STACK_HEURISTIC 32

void syscall_init (void);
//extern struct lock file_lock;

#endif /* userprog/syscall.h */
