#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#define UNMAPPED(i) (i)==0x10123420 ||(i)==0x20101234
#define UNMAP(i) (i)==0xc0100000 ||(i)==0x10123420

void syscall_init (void);
struct lock filesys_lock;

#endif /* userprog/syscall.h */
