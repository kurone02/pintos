#ifndef FILESYS_FILESYS_H
#define FILESYS_FILESYS_H

#include <stdbool.h>
#include "filesys/off_t.h"

#define MAX_FILE 64

/* Sectors of system file inodes. */
#define FREE_MAP_SECTOR 0       /* Free map file inode sector. */
#define ROOT_DIR_SECTOR 1       /* Root directory file inode sector. */

/* Block device that contains the file system. */
extern struct block *fs_device;

void filesys_init (bool format);
void filesys_done (void);
bool filesys_create (const char *name, off_t initial_size);
struct file *filesys_open (const char *name);
bool filesys_remove (const char *name);

void close_all_files();

struct file *get_file(int);
bool create_file(const char*, off_t);
bool remove_file(const char*);
int open_file(const char*, bool);
off_t get_file_size(int );
off_t read_file(int, void*, off_t);
off_t write_to_file(int, const void*, off_t);
void seek_file(int, off_t);
off_t cur_pos_file(int);
void close_file(int);

#endif /* filesys/filesys.h */
