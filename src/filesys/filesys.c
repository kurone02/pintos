#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"
#include "lib/stdio.h"
#include "lib/kernel/stdio.h"
#include "devices/input.h"
#include "inode.h"

/* Partition that contains the file system. */
struct block *fs_device;

/* Global lock for file system */
static struct lock filesys_lock;

/* Acquire the lock for file system */
static void acquire_filesys_lock() {
  lock_acquire(&filesys_lock);
}

/* Release the lock for file system */
static void release_filesys_lock() {
  lock_release(&filesys_lock);
}

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  lock_init (&filesys_lock);
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  block_sector_t inode_sector = 0;
  struct dir *dir = dir_open_root ();
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add (dir, name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  struct dir *dir = dir_open_root ();
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, name, &inode);
  dir_close (dir);

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct dir *dir = dir_open_root ();
  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

/* Close all file descriptor */
void
close_all_files() {
  for(int i = 2; i < MAX_FILE; i++) {
    close_file(i);
  }
}

/* To get the file from file descriptor */
struct file*
get_file(int fd) {
  /* The first two fd are reserved for stdin and stdout */
  if(fd < 2 || fd > MAX_FILE) {
    return NULL;
  }

  struct thread *cur_thread = thread_current();
  return cur_thread->fdt[fd];
}

/* To create file from process */
bool
create_file(const char *name, off_t initial_size) {
  acquire_filesys_lock();
  int success = filesys_create(name, initial_size);
  release_filesys_lock();
  return success;
}

/* To remove file from process */
bool
remove_file(const char *name) {
  acquire_filesys_lock();
  int success = filesys_remove(name);
  release_filesys_lock();
  return success;
}

/* To open file from process */
int
open_file(const char *name, bool executable) {
  acquire_filesys_lock();
  struct file *f = filesys_open(name);
  release_filesys_lock();

  if(f == NULL) {
    return -1;
  }

  if(executable) {
    file_deny_write(f);
  }

  /* Find a available file descriptor */
  struct thread *cur_thread = thread_current();
  for(int i = 2; i < MAX_FILE; i++) {
    if(cur_thread->fdt[i] == NULL) {
      cur_thread->fdt[i] = f;
      // printf("fd should be %d\n", i);
      return i;
    }
  }

  // printf("fd should be %d\n", -1);

  // /* Not able to find available file descriptor */
  // acquire_filesys_lock();
  // printf("Finish closing1\n");
  // file_close(name);
  // printf("Finish closing2\n");
  // release_filesys_lock();
  // printf("Finish closing3\n");
  return -1;
}

/* To get the file size */
off_t
get_file_size(int fd) {
  struct file *f = get_file(fd);

  if(f == NULL) {
    return 0;
  }
  
  off_t size = 0;
  acquire_filesys_lock();
  size = file_length(f);
  release_filesys_lock();

  return size;
}

/* To read the file from process */
off_t
read_file(int fd, void *buffer_, off_t size) {
  char *buffer = buffer_;
  off_t bytes_read = 0;

  if(fd == STDIN_FILENO) {
    while(bytes_read < size) {
      char c = input_getc();
      if(c == '\n') {
        break;
      }
      buffer[bytes_read++] = c;
    }

    return bytes_read;
  }

  struct file *f = get_file(fd);
  if(f == NULL) {
    return 0;
  }

  acquire_filesys_lock();
  bytes_read = file_read(f, buffer, size);
  release_filesys_lock();

  return bytes_read;
}

/* To write to the file from process */
off_t
write_to_file(int fd, const void *buffer, off_t size) {
  if(fd == STDOUT_FILENO) {
    putbuf(buffer, size);
    return size;
  }

  struct file *f = get_file(fd);

  if(f == NULL) {
    return 0;
  }

  acquire_filesys_lock();
  off_t byte_written = file_write(f, buffer, size);
  release_filesys_lock();

  return byte_written;
}

/* To seek in the file given a position */
void
seek_file(int fd, off_t new_pos) {
  struct file *f = get_file(fd);
  if(f == NULL) {
    return;
  }
  acquire_filesys_lock();
  file_seek(f, new_pos);
  release_filesys_lock();
}

/* Get the current position in the file */
off_t
cur_pos_file(int fd) {
  struct file *f = get_file(fd);
  if(f == NULL) {
    return 0;
  }
  acquire_filesys_lock();
  off_t cur_pos = file_tell(f);
  release_filesys_lock();
  return cur_pos;
}

/* Close the file from process */
void
close_file(int fd) {
  struct thread *cur_thread = thread_current();
  struct file *f = get_file(fd);

  if(f == NULL) {
    return;
  }

  acquire_filesys_lock();
  file_close(f);
  release_filesys_lock();
  cur_thread->fdt[fd] = NULL;
}