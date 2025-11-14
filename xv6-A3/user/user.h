/*
AAKASH JANA AAJ284 11297343
NANDISH JHA NAJ474 11282001
*/
#define SBRK_ERROR ((char *)-1)

/* CMPT 332 Nandish Jha NAJ474 11282001 2025 change */
typedef unsigned int uint;
struct stat;
/* CMPT 332 GROUP 01 Change, Fall 2025 */
struct sched_attr {
  int deadline;
  int priority;
  int start_time;  /* absolute start time in ticks */
};
/* CMPT 332 GROUP 01 Change, Fall 2025 */

// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int*);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(const char*, char**);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
int getpid(void);
char* sys_sbrk(int,int);
int pause(int);
int uptime(void);
int trace(int mask); /* CMPT 332 Nandish Jha NAJ474 11282001 2025 change */
int mtx_create(int locked); /* CMPT 332 GROUP 01 Change, Fall 2025 */
int mtx_lock(int lock_id); /* CMPT 332 GROUP 01 Change, Fall 2025 */
int mtx_unlock(int lock_id); /* CMPT 332 GROUP 01 Change, Fall 2025 */
int get_num_free_pages(void); /* CMPT 332 GROUP 01 Change, Fall 2025 - A3*/
int setSchedAttr(struct sched_attr* attr); /* CMPT 332 GROUP 01 Change,
                                            * Fall 2025 */
int getSchedAttr(struct sched_attr* attr); /* CMPT 332 GROUP 01 Change,
                                            * Fall 2025 */
int sleep(int); /* CMPT 332 GROUP 01 Change, Fall 2025 - A3 */

// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
void *memmove(void*, const void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
char* gets(char*, int max);
uint strlen(const char*);
void* memset(void*, int, uint);
int atoi(const char*);
int memcmp(const void *, const void *, uint);
void *memcpy(void *, const void *, uint);
char* sbrk(int);
char* sbrklazy(int);

// printf.c
void fprintf(int, const char*, ...) __attribute__ ((format (printf, 2, 3)));
void printf(const char*, ...) __attribute__ ((format (printf, 1, 2)));

// umalloc.c
void* malloc(uint);
void free(void*);
