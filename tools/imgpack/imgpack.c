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
	char 	name[IH_NMLEN];	/* Image Name		*/
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


static  const char *
get_filename(const char *fpath)
{
#if 1
	int i;
	const char *filename = fpath;
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
    closedir(d);
	return count;
}


static int img_unpack( char* path_src, char* path_dest){
	FILE *fd_src = NULL;
	FILE *fd_dest = NULL;
	char *header_buff = NULL;
	char *data_buff = NULL;
	struct pack_header *pack_header_p;
	char file_path[256];
	int header_size = sizeof(struct pack_header);
	long pos = 0;

	fd_src = fopen(path_src, "rb");
	if(NULL==fd_src){
		fprintf(stderr, "fopen %s failed, %s\n",path_src, strerror(errno));
		return -1;
	}
	header_buff = malloc(header_size);
	if(NULL==header_buff){
		fprintf(stderr, "malloc header_buff failed, %s\n", strerror(errno));
		return -1;
	}


	do{
		memset(header_buff, 0 ,header_size);

		fseek(fd_src,pos,SEEK_SET);
		if(fread((void *)header_buff, 1, header_size, fd_src)!=header_size){
			fprintf(stderr,"short read(%d) reading from %s (%s)\n",
					header_size, path_src, strerror(errno));
        	return -1;
		}

		pack_header_p = (struct pack_header*)header_buff;
		if(pack_header_p->magic != IH_MAGIC)
		{
			fprintf(stderr,"%s: wrong pack img! magic == 0x%x \n", path_dest,pack_header_p->magic);
			return  -1;
		}
		pos = pack_header_p->next ;
		data_buff = malloc(pack_header_p->size);
		if(NULL==data_buff){
			fprintf(stderr, "malloc data_buff [%d] failed, %s\n", pack_header_p->size,strerror(errno));
			return -1;
		}
		fread((void *)data_buff, 1, pack_header_p->size, fd_src);


		sprintf(file_path, "%s/%s", path_dest, pack_header_p->name);

		fd_dest = fopen(file_path, "wb+");
		if(NULL == fd_dest)
		{
			fprintf(stderr,"open %s failed: %s\n", pack_header_p->name, strerror(errno));
			return -1;
		}

		if(fwrite((void *)(data_buff), 1, pack_header_p->size, fd_dest) != pack_header_p->size)
		{
			fprintf(stderr,"short write(%d) writing to %s (%s)\n",
        			pack_header_p->size, pack_header_p->name, strerror(errno));
        	return -1;
    	}
		fclose(fd_dest);
		free(data_buff);
		fd_dest = NULL;
		data_buff = NULL;
	}while(pack_header_p->next!=0);

	fclose(fd_src);

	return 0;
}


static int img_pack(char* path_src,char* path_dest){
	DIR *dir = NULL;
	FILE *fd_src = NULL;
	FILE *fd_dest = NULL;
	char *data_buff = NULL;
	struct dirent *de = NULL;
	struct pack_header *pack_header_p = NULL;
	int nums = 0;
	int i = 0;
	unsigned int pos = 0;
	size_t fsize_src,wrote_size;
	char file_path[256];
	const char *filename = NULL;
	nums = get_dir_filenums(path_src);
	dir = opendir(path_src);
	if(NULL == dir) {
		fprintf(stderr, "opendir %s failed, %s\n",path_src, strerror(errno));
		return -1;
	}

	fd_dest = fopen(path_dest, "wb+");
	if(NULL == fd_dest)	{
		fprintf(stderr,"open %s failed: %s\n", path_dest, strerror(errno));
		return -1;
	}

	while((de = readdir(dir)) != 0){
		if(de->d_name[0] == '.')
			continue;
		sprintf(file_path, "%s/%s", path_src, de->d_name);

		filename = de->d_name;

		fd_src = fopen(file_path, "rb");
		if(NULL == fd_src){
			fprintf(stderr,"%s: %s\n", path_dest, strerror(errno));
			return -1;
		}


		fsize_src= get_filesize(file_path);
		wrote_size = sizeof(struct pack_header) + fsize_src + 16 - (fsize_src%16);	//align 16 byte
		data_buff = malloc(wrote_size);
		if(NULL==data_buff){
			fprintf(stderr, "malloc data_buff [%d] failed, %s\n", wrote_size,strerror(errno));
			return -1;
		}

		memset(data_buff, 0, wrote_size);

		pack_header_p = (struct pack_header *)data_buff;
		pack_header_p->magic = IH_MAGIC;
		pack_header_p->index = i;
		pack_header_p->nums = nums;
		pack_header_p->start = pos + sizeof(struct pack_header);
		pack_header_p->size = fsize_src;
		pos += wrote_size;

		if(pack_header_p->index == (nums-1)){
			pack_header_p->next = 0;
		}else{
			pack_header_p->next = pos;
		}

		strncpy(pack_header_p->name, filename, IH_NMLEN);
		pack_header_p->name[IH_NMLEN-1] = 0;

		fread((void *)(data_buff+sizeof(struct pack_header)), 1, fsize_src, fd_src);

		if(fwrite((void *)data_buff, 1, wrote_size, fd_dest) != wrote_size)
		{
			printf("short write(%d) writing to %s (%s)\n",
				wrote_size, path_dest, strerror(errno));
			return -1;

		}
		i++;
		free(data_buff);
		data_buff = NULL;
		fclose(fd_src);
		fd_src = NULL;
	}
	fclose(fd_dest);
	fd_dest = NULL;
	closedir(dir);

	return 0;
}



static int img_pack1(char** filelist,char* path_dest,int count){
	DIR *dir = NULL;
	FILE *fd_src = NULL;
	FILE *fd_dest = NULL;
	const char *path_src = NULL;
	char *data_buff = NULL;
	struct pack_header *pack_header_p = NULL;

	int i = 0;
	unsigned int pos = 0;
	size_t fsize_src,wrote_size;
	const char *filename = NULL;


	fd_dest = fopen(path_dest, "wb+");
	if(NULL == fd_dest)	{
		fprintf(stderr,"open %s failed: %s\n", path_dest, strerror(errno));
		return -1;
	}

	while(i < count){

		path_src = *(filelist+i);

		filename = get_filename(path_src);
		printf("path_src==%s [%s]\n",path_src,*filelist);
		fd_src = fopen(path_src, "rb");
		if(NULL == fd_src){
			fprintf(stderr,"%s: %s\n", path_src, strerror(errno));
			return -1;
		}


		fsize_src= get_filesize(path_src);
		wrote_size = sizeof(struct pack_header) + fsize_src + 16 - (fsize_src%16);	//align 16 byte
		data_buff = malloc(wrote_size);
		if(NULL==data_buff){
			fprintf(stderr, "malloc data_buff [%d] failed, %s\n", wrote_size,strerror(errno));
			return -1;
		}

		memset(data_buff, 0, wrote_size);

		pack_header_p = (struct pack_header *)data_buff;
		pack_header_p->magic = IH_MAGIC;
		pack_header_p->index = i;
		pack_header_p->nums = count;
		pack_header_p->start = pos + sizeof(struct pack_header);
		pack_header_p->size = fsize_src;
		pos += wrote_size;

		if(pack_header_p->index == (count-1)){
			pack_header_p->next = 0;
		}else{
			pack_header_p->next = pos;
		}

		strncpy(pack_header_p->name, filename, IH_NMLEN);
		pack_header_p->name[IH_NMLEN-1] = 0;

		fread((void *)(data_buff+sizeof(struct pack_header)), 1, fsize_src, fd_src);

		if(fwrite((void *)data_buff, 1, wrote_size, fd_dest) != wrote_size)
		{
			printf("short write(%d) writing to %s (%s)\n",
				wrote_size, path_dest, strerror(errno));
			return -1;

		}
		i++;
		free(data_buff);
		data_buff = NULL;
		fclose(fd_src);
		fd_src = NULL;
	}
	fclose(fd_dest);
	fd_dest = NULL;
	closedir(dir);

	return 0;
}

static char *doc = "Amlogic `imgpack' usage:\n\
\n\
  # unpack files to the archive\n\
  imgpack -d [archive]  [destination-directory]\n\
  # pack files in directory to the archive\n\
  imgpack -r [source-directory]  [archive]\n\
  # pack files to the archive\n\
  imgpack  [file1].. [fileN]  [archive]\n";

int main(int argc, char **argv)
{

	int ret = 0;
	int c = 0;
	char* path_src = NULL;
	int (*pack) (char*, char*) = NULL;


	if(argc < 3)
	{
		printf("invalid arguments\n");
		printf("%s",doc);
		exit(-1);
	}


	while((c = getopt(argc, argv, "d:r:")) != -1)
	{
		switch(c)
		{
			case 'd':
				path_src = optarg;
				if(pack){
					printf("%s",doc);
					exit(-1);
				}

				pack = img_unpack;
				break;
			case 'r':
				path_src = optarg;
				if(pack){
					printf("%s",doc);
					exit(-1);
				}
				pack = img_pack;
				break;

			case '?':
				pack = NULL;
				exit(-1);
			    break;
			default:
				ret = -1;
				break;
		}
	}

	if(pack){
		ret = pack(path_src,*(argv+argc-1));
		exit(ret);
	}

	ret = img_pack1(argv+1,*(argv+argc-1),argc-2);

	exit(ret);
}
