#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
/*****************proj#2*/
void process_wake_parent (int status);

struct mmap_file{
	struct sup_page_entry *spte;
	int mapid;
	struct list_elem elem;
};

bool install_page(void *upage, void *kpage, bool writable);
bool process_add_mmap( struct sup_page_entry *spte);
//void process_remove_mmap (int mappint);
//struct lock file;
/*
struct parent_child{
  struct thread *parent_thread;
  tid_t parent_tid;
  tid_t child_tid;
  bool waiting;
  int status;
  struct list_elem elem;
	struct semaphore load;
	struct semaphore exit;
};
*/
#endif /* userprog/process.h */
