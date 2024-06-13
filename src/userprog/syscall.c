#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include <string.h>
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "lib/stdio.h"
#include "lib/kernel/stdio.h"

#define SET_RETURN_VALUE(x) f->eax = x

static void syscall_handler (struct intr_frame *);

/* Utility function */

static int get_user(const uint8_t*);
static bool put_user(uint8_t*, uint8_t);
static void error_exit(struct intr_frame*);
static int get_argc(void*);
static void *get_args(void *);

/* For retriving data */

static bool read_byte(const uint8_t*, uint8_t*);
static bool read_int(const uint8_t*, int*);
static bool read_char(const uint8_t*, char *);
static bool get_arg_int(const uint8_t*, int, int*);
static bool get_arg_ptr(const uint8_t*, int, uint8_t**);
static bool get_arg_str(const uint8_t*, int, char**);

/* Systemcall handlers */

static void halt(void);
static void exit(const void*, struct intr_frame*);
static void exec(const void*, struct intr_frame*);
static void wait(const void*, struct intr_frame*);

static void write(const void *, struct intr_frame*);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  // printf ("system call!\n");

  char **args;
  void *stack_pointer = f->esp;
  int number;

  if(!get_arg_int(stack_pointer, 0, &number)) {
    thread_exit ();
  }

  // hex_dump(stack_pointer, stack_pointer, PHYS_BASE - stack_pointer, true);
  // printf("The current stack pointer: %p\n", stack_pointer);
  // printf("The number: %d\n", number);

  switch(number) {
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      args = get_args(stack_pointer);
      exit(args, f);
      break;
    case SYS_EXEC:
      args = get_args(stack_pointer);
      exec(args, f);
      break;
    case SYS_WAIT:
      args = get_args(stack_pointer);
      wait(args, f);
      break;

    /* TODO */

    case SYS_WRITE:
      args = get_args(stack_pointer);
      write(args, f);
      break;
    default:
      break;
  }
}


/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user (const uint8_t *uaddr) {
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}

/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte) {
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}

/* Read a byte from the given address, return true if the address is valid,
  false otherwise. The value is stored in aux
*/
static bool
read_byte (const uint8_t *addr, uint8_t *aux) {
  if(is_kernel_vaddr(addr)) {
    return false;
  }

  int byte = get_user (addr);
  if (byte == -1) {
    return false;
  }

  *aux = (uint8_t) byte;
  return true;
}

/* Read an int from the given address, return true if the address is valid,
  false otherwise. The value is stored in aux
*/
static bool
read_int (const uint8_t *addr, int *aux) {

  /* Outside valid range. */
  if (is_kernel_vaddr (addr) || is_kernel_vaddr (addr + sizeof (int)))
    return false;

  int byte0 = get_user(addr);
  int byte1 = get_user(addr + 1);
  int byte2 = get_user(addr + 2);
  int byte3 = get_user(addr + 3);

  // printf("read_int: %d %d %d %d\n", b0, b1, b2, b3);

  if (byte0 == -1 || byte1 == -1 || byte2 == -1 || byte3 == -1)
    return false;

  /* Combine 4-bytes into a 32-bit integer */
  *aux = (uint8_t) byte0        | 
         (uint8_t) byte1 << 8   | 
         (uint8_t) byte2 << 16  | 
         (uint8_t) byte3 << 24;
  return true;
}

/* Read a char from the given address, return true if the address is valid,
  false otherwise. The value is stored in aux
*/
static bool
read_char (const uint8_t *addr, char *aux) {
  return read_byte(addr, (uint8_t *)aux);
}

/* Read an integer argument from the given address, return true if the address is valid,
  false otherwise. The value is stored in aux
*/
static bool
get_arg_int (const uint8_t *addr, int pos, int *aux) {
  return read_int(addr + sizeof(int) * pos, aux);
}

/* Read a pointer argument from the given address, return true if the address is valid,
  false otherwise. The value is stored in aux
*/
static bool
get_arg_ptr (const uint8_t *addr, int pos, uint8_t **aux) {
  return get_arg_int(addr, pos, (int*)aux);
}

/* Read a string argument from the given address, return true if the address is valid,
  false otherwise. The value is stored in aux
*/
static bool
get_arg_str (const uint8_t *addr, int pos, char **aux) {
  if (!get_arg_int(addr, pos, (int*)aux)) {    
    return false;
  }

  uint8_t *ustraddr = (uint8_t *) *aux;
  char c;
  while(read_char (ustraddr++, &c) && c);
  return c == '\0';
}

/* Get the number of arguments */
static int 
get_argc(void* stack_ptr) {
  return *(int*)(stack_ptr + sizeof(void*));
}

/* Get the pointer to the arguments */
static void*
get_args(void *stack_ptr) {
  return (stack_ptr + sizeof(void*));
}

/** Exit process when error */
static void
error_exit(struct intr_frame *f) {
  struct thread* cur_thread = thread_current();
  cur_thread->exit_status = -1;
  SET_RETURN_VALUE(-1);
  printf("%s: exit(%d)\n", cur_thread->name, cur_thread->exit_status);
  thread_exit();
}


/** Shutdown pintos */
static void
halt(void) {
  shutdown_power_off();
}

/** Exit process */
static void
exit(const void *args, struct intr_frame *f) {
  int exit_code;
  if(!get_arg_int(args, 0, &exit_code)) {
    error_exit(f);
  }
  struct thread* cur_thread = thread_current();
  cur_thread->exit_status = exit_code;
  SET_RETURN_VALUE(exit_code);
  printf("%s: exit(%d)\n", cur_thread->name, cur_thread->exit_status);
  thread_exit();
}

/* Create a new process to run the command line */
static void 
exec(const void *args, struct intr_frame *f) {
  char *cmd_line;

  if(!get_arg_str(args, 0, &cmd_line)) {
    error_exit(f);
  }

  tid_t tid = process_execute(cmd_line);  
  SET_RETURN_VALUE(tid);
}

/* Wait for the process to finish */
static void 
wait(const void* args, struct intr_frame*f) {
  pid_t process_id;

  if(!get_arg_int(args, 0, &process_id)) {
    error_exit(f);
  }

  int tid = process_wait(process_id);
  
  SET_RETURN_VALUE(tid);
}

/* Write something */
static void
write(const void *args, struct intr_frame *f) {
  int fd;
  char *buffer;
  unsigned size;

  if(!get_arg_int(args, 0, &fd) || 
     !get_arg_ptr(args, 1, (uint8_t **)&buffer) || 
     !get_arg_int(args, 2, (int *)&size)) {
    error_exit(f);
  }

  int written = 0;
  if (fd == STDOUT_FILENO) {
    putbuf (buffer, size);
    written = size;
  }

  SET_RETURN_VALUE(written);
}