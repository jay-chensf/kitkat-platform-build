/*
 * Command for pack imgs.
 *
 * Copyright (C) 2012 Amlogic.
 * Elvis Yu <elvis.yu@amlogic.com>
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
//#include <sys/mman.h> 
#include <sys/types.h> 
//#include <linux/types.h>


#define IH_MAGIC	0x27051956	/* Image Magic Number		*/
#define IH_NMLEN		32	/* Image Name Length		*/


struct pack_header{
	unsigned int 	magic;	/* Image Header Magic Number	*/
	unsigned int 	hcrc;	/* Image Header CRC Checksum	*/
	unsigned int	size;	/* Image Data Size		*/
	unsigned int	start;	/* Data	 Load  Address		*/
	unsigned int	end;		/* Entry Point Address		*/
	unsigned int	next;	/* Image Creation Timestamp	*/
	unsigned int	dcrc;	/* Image Data CRC Checksum	*/
	unsigned char	index;		/* Operating System		*/
	unsigned char	nums;	/* CPU architecture		*/
	unsigned char   type;	/* Image Type			*/
	unsigned char 	comp;	/* Compression Type		*/
	unsigned char 	name[IH_NMLEN];	/* Image Name		*/
} __attribute__ ((packed));


struct file_list{
	char *name[256];
	struct file_list *next;
};


static size_t
get_filesize(const char *fpath) 
{ 
	struct stat buf; 
	if(stat(fpath, &buf) < 0) 
	{ 
		fprintf (stderr, "Can't stat %s : %s\n", fpath, strerror(errno));
		exit (EXIT_FAILURE); 
	} 
	return buf.st_size; 
}

static char *
get_filename(const char *fpath) 
{ 
#if 1
	int i;
	char *filename = fpath;
	for(i=strlen(fpath)-1; i>=0; i--)
	{
		if(fpath[i] == '/')
		{
			i++;
			filename = fpath + i;
			break;
		}
	}
	
	return filename; 
#else
	return (strrchr(fpath, '/')+1);
#endif
}

int get_dir_filenums(const char *dir_path)
{
	int count = 0;
	DIR *d;
	struct dirent *de;
	d = opendir(dir_path);
    if(d == 0) {
        fprintf(stderr, "opendir failed, %s\n", strerror(errno));
        return -1;
    }
	while((de = readdir(d)) != 0){
	        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
	        if(de->d_name[0] == '.') continue;
		count++;
	}
	return count;	
}




int main(int argc, char *argv[])
{
	int c, i;
	FILE *f_src = NULL;
	FILE *f_dest = NULL;
	char *fpath_src, *fpath_dest;
	size_t fsize_src, fsize_dest, wrote_size;
	struct pack_header *pack_header_p;
	char buff[4*1024*1024];
	char *filename;
	char file_path[256];
	unsigned int pos;
	unsigned char nums;
	int is_dir = 0, ret = 0;

	if(argc < 3)
	{
		printf("invalid arguments\n");
		exit(-1);
	}
	
	memset(buff, 0 ,sizeof(buff));

	fpath_dest = argv[argc-1];
	f_dest = fopen(fpath_dest, "wb+");
	if(f_dest == (FILE *)NULL)
	{
		printf("%s: %s\n", fpath_dest, strerror(errno));
	}
	
	while((c = getopt(argc, argv, "d:r")) != -1)
	{
		switch(c)
		{
			case 'd':
			{
				fread((void *)buff, 1, fsize_src, f_src);
				pos = 0;
				while(1)
				{
					pack_header_p = (struct pack_header *)(buff+pos);
					if(pack_header_p->magic != IH_MAGIC)
					{
						printf("%s: wrong pack img!\n", fpath_dest);
						ret = -1;
						goto finish;
					}

					f_dest = fopen(pack_header_p->name, "wb+");
					if(f_dest == (FILE *)NULL)
					{
						printf("%s: %s\n", pack_header_p->name, strerror(errno));
					}

					pos += sizeof(struct pack_header);
					wrote_size = pack_header_p->size;
					if(fwrite((void *)(buff + pos), 1, wrote_size, f_dest) != wrote_size)
					{
	                	printf("short write(%d) writing to %s (%s)\n",
							wrote_size, pack_header_p->name, strerror(errno));
	                	ret = -1;
						goto finish;
	            	}

					fclose(f_dest);
					f_dest = NULL;
					if(pack_header_p->next == 0)
					{
						break;
					}
					else
					{
						pos = pack_header_p->next;
					}
				}
				break;
			}
			case 'r':
			{
    			is_dir = 1;
				break;
			}
			default:
				break;
				ret = -1;
				goto finish;
		}
	}

	if(is_dir)
	{
		DIR *d;
		struct dirent *de;
		fpath_src = argv[2];
		nums = get_dir_filenums(fpath_src);
	    d = opendir(fpath_src);
	    if(d == 0) {
	        fprintf(stderr, "opendir failed, %s\n", strerror(errno));
	        return -1;
	    }
		i = 0;
	    while((de = readdir(d)) != 0){
	        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
	        if(de->d_name[0] == '.') continue;
			sprintf(file_path, "%s/%s", fpath_src, de->d_name);
			filename = de->d_name;

			f_src = fopen(file_path, "rb");
			if(f_dest == (FILE *)NULL)
			{
				printf("%s: %s\n", fpath_dest, strerror(errno));
			}

			fsize_src= get_filesize(file_path);
			wrote_size = sizeof(struct pack_header) + fsize_src + 16 - (fsize_src%16);	//align 16 byte
			memset(buff, 0, wrote_size);
			
			
			pack_header_p = (struct pack_header *)buff;
			pack_header_p->magic = IH_MAGIC;
			pack_header_p->index = i;
			pack_header_p->nums = nums;
			pack_header_p->start = pos + sizeof(struct pack_header);
			pack_header_p->size = fsize_src;
			pos += wrote_size;
			if(pack_header_p->index == (nums-1))
			{

				pack_header_p->next = 0;
			}
			else
			{
				pack_header_p->next = pos;
			}

			strncpy(pack_header_p->name, filename, IH_NMLEN);
			pack_header_p->name[IH_NMLEN-1] = 0;
			
			fread((void *)(buff+sizeof(struct pack_header)), 1, fsize_src, f_src);
			
			if(fwrite((void *)buff, 1, wrote_size, f_dest) != wrote_size)
			{
		    	printf("short write(%d) writing to %s (%s)\n",
					wrote_size, fpath_dest, strerror(errno));
		    	ret = -1;
				goto finish;
			}
			i++;
	    }
		goto finish;
	}

	pos = 0;
	nums = argc - 2;
	for(i=1; i<argc-1; i++)
	{
		fpath_src = argv[i];
		f_src = fopen(fpath_src, "rb");
		if(f_dest == (FILE *)NULL)
		{
			printf("%s: %s\n", fpath_dest, strerror(errno));
		}

		fsize_src= get_filesize(fpath_src);
		wrote_size = sizeof(struct pack_header) + fsize_src + 16 - (fsize_src%16);	//align 16 byte
		memset(buff, 0, wrote_size);
		
		pack_header_p = (struct pack_header *)buff;
		pack_header_p->magic = IH_MAGIC;
		pack_header_p->index = i - 1;
		pack_header_p->nums = nums;
		
		pack_header_p->start = pos + sizeof(struct pack_header);
		pack_header_p->size = fsize_src;
		pos += wrote_size;
		if(pack_header_p->index == (nums-1))
		{
			pack_header_p->next = 0;
		}
		else
		{
			pack_header_p->next = pos;
		}
		filename = get_filename(fpath_src);
		strncpy(pack_header_p->name, filename, IH_NMLEN);
		pack_header_p->name[IH_NMLEN-1] = 0;

		fread((void *)(buff+sizeof(struct pack_header)), 1, fsize_src, f_src);
		
		if(fwrite((void *)buff, 1, wrote_size, f_dest) != wrote_size)
		{
	    	printf("short write(%d) writing to %s (%s)\n",
				wrote_size, fpath_dest, strerror(errno));
	    	ret = -1;
			goto finish;
		}
	}
finish:
	if(f_src)
	{
		fclose(f_src);
	}
	if(f_dest)
	{
		fclose(f_dest);
	}

	exit(ret);
}
