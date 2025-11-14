/*
AAKASH JANA AAJ284 11297343
NANDISH JHA NAJ474 11282001
*/

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

void
find(char *path, char *target)
{
	int fd;
	struct dirent de;
	struct stat st;

	/* Try to open the given path for reading */
	if((fd = open(path, O_RDONLY)) < 0)
    {
		printf("find: cannot open %s\n", path);
		return;
	}

	/* Get file/directory information using fstat */
	if(fstat(fd, &st) < 0)
    {
		printf("find: cannot stat %s\n", path);
		close(fd);
		return;
	}

    if(st.type == T_FILE) /* Check if path is a regular file */
    {
        /* Extract the filename from the path */
        char *filename = path;
        for(int i = strlen(path) - 1; i >= 0; i--)
        {
            if(path[i] == '/')
            {
                filename = path + i + 1;
                break;
            }
        }
        /* Print only if filename matches target */
        if(strcmp(filename, target) == 0)
        {
            printf("%s\n", path);
        }
    }
    else if(st.type == T_DIR)
    {
        /* Buffer for building new paths */
        char buf[512], *p;
        /* printf("Directory entries in %s:\n", path); */
        /* Read each directory entry */
        while(read(fd, &de, sizeof(de)) == sizeof(de))
        {
            /* Skip empty directory entries */
            if(de.inum == 0)
                    continue;
            /* Skip "." and ".." to avoid infinite recursion */
            if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                    continue;
            /* Build the new path for the entry */
            if(strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf))
            {
                /* Path too long, skip */
                continue;
            }
            strcpy(buf, path);
            p = buf + strlen(buf);
            *p++ = '/';
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            /* Recursively call find on the new path */
            find(buf, target);
        }
    }
	/* Close the file descriptor */
	close(fd);
}

int
main(int argc, char *argv[])
{
    /* Check for correct number of arguments */
	if(argc != 3)
    {
		printf("Usage: find <path> <filename>\n");
		exit(1);
	}
	/* Call find function with path and target filename */
	find(argv[1], argv[2]);
	exit(0);
}
