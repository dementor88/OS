#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <user/syscall.h>

void syscall_init (void);

static	void		__halt		(void) __attribute__ ((noreturn));
static 	void		__exit		(int status) __attribute ((noreturn));
static	pid_t		__exec		(const char *file);
static	int			__wait		(pid_t pid);
static	bool		__create	(const char *file, unsigned initial_size);
static	bool		__remove	(const char *file);
static	int			__open		(const char *file);
static	int			__filesize	(int fd);
static	int			__read		(int fd, void *buffer, unsigned length);
static	int			__write		(int fd, const void* buffer, unsigned length);
static	void		__seek		(int fd, unsigned position);
static	unsigned 	__tell		(int fd);
static	void		__close		(int fd);


#endif /* userprog/syscall.h */
