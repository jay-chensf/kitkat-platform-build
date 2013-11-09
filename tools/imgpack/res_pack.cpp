/*
 * Command for pack imgs.
 *
 * Copyright (C) 2012 Amlogic.
 * Elvis Yu <elvis.yu@amlogic.com>
 */
#include "res_pack_i.h"

#define COMPILE_TYPE_CHK(expr, t)       typedef char t[(expr) ? 1 : -1]

COMPILE_TYPE_CHK(AML_RES_IMG_HEAD_SZ == sizeof(AmlResImgHead_t), a);//assert the image header size 64
COMPILE_TYPE_CHK(AML_RES_ITEM_HEAD_SZ == sizeof(AmlResItemHead_t), b);//assert the item head size 64

#define IMG_HEAD_SZ     sizeof(AmlResImgHead_t)
#define ITEM_HEAD_SZ    sizeof(AmlResItemHead_t)
//#define ITEM_READ_BUF_SZ    (1U<<20)//1M
#define ITEM_READ_BUF_SZ    (64U<<10)//64K to test


typedef int (*pFunc_getFile)(const char** const , __hdle *, char* );

static size_t get_filesize(const char *fpath)
{
	struct stat buf;
	if(stat(fpath, &buf) < 0)
	{
		fprintf (stderr, "Can't stat %s : %s\n", fpath, strerror(errno));
		exit (EXIT_FAILURE);
	}
	return buf.st_size;
}


static  const char* get_filename(const char *fpath)
{
#if 1
	int i;
	const char *filename = fpath;
	for(i=strlen(fpath)-1; i>=0; i--)
	{
		if('/' == fpath[i] || '\\' == fpath[i])
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


int get_file_path_from_argv(const char** const argv, __hdle *hDir, char* fileName)
{
    long index = (long)hDir;
    const char* fileSrc = argv[index];

    strcpy(fileName, fileSrc);
    return 0;
}

#if defined(WIN32)
static __hdle _find_first_file(const char* const dirPath, WIN32_FIND_DATA& findFileData)
{
    HANDLE hdle = INVALID_HANDLE_VALUE;
    TCHAR szDir[MAX_PATH];
    size_t length_of_arg;
    // Check that the input path plus 3 is not longer than MAX_PATH.
    // Three characters are for the "\*" plus NULL appended below.
    StringCchLength(dirPath, MAX_PATH, &length_of_arg);

    if (length_of_arg > (MAX_PATH - 3))
    {
        _tprintf(TEXT("\nDirectory path is too long.\n"));
        return NULL;
    }

    _tprintf(TEXT("\nTarget directory is %s\n\n"), dirPath);

    // Prepare string for use with FindFile functions.  First, copy the
    // string to a buffer, then append '\*' to the directory name.

    StringCchCopy(szDir, MAX_PATH, dirPath);
    StringCchCat(szDir, MAX_PATH, TEXT("\\*"));

    hdle = FindFirstFile(szDir, &findFileData);
    if(INVALID_HANDLE_VALUE == hdle){
        errorP("Fail to open dir\n");
        return NULL;
    }

    return hdle;
}

int traverse_dir(const char** const dirPath, __hdle *hDir, char* filePath)
{
  WIN32_FIND_DATA findFileData;
  HANDLE hdle = INVALID_HANDLE_VALUE;
  int ret = 0;

  if (!*hDir)//not open yet!
  {
      hdle = _find_first_file(*dirPath, findFileData);
      if(INVALID_HANDLE_VALUE == hdle){
          errorP("Fail to find first file in dir(%s)\n", *dirPath);
          return __LINE__;
      }
      *hDir = hdle;
      if(strcmp(".", findFileData.cFileName) && strcmp("..", findFileData.cFileName))goto _ok;
  }

  hdle = *hDir;
  do 
  {
    ret = !FindNextFile(hdle, &findFileData);
    if(ret){
      debugP("Find end.\n");
      FindClose(hdle);
      return __LINE__;
    }
  } while(!strcmp(".", findFileData.cFileName) || !strcmp("..", findFileData.cFileName) );
  
_ok: 
  //debugP("file %s\n", findFileData.cFileName);
  if(MAX_PATH < strlen(findFileData.cFileName)){
    errorP("Buffer samll\n");
    FindClose(hdle);
    return __LINE__;
  }
  sprintf(filePath, "%s\\%s", *dirPath, findFileData.cFileName);
  return ret;
}

int get_dir_filenums(const char * const dirPath)
{
    WIN32_FIND_DATA findFileData;
    HANDLE hdle = INVALID_HANDLE_VALUE;
    int fileNum = 0;
    int ret = 0;
    TCHAR szDir[MAX_PATH];
    size_t length_of_arg;

    hdle = _find_first_file(dirPath, findFileData);
    if(INVALID_HANDLE_VALUE == hdle){
        errorP("Fail to open dir %s\n", szDir);
        return __LINE__;
    }

    do {
        if(strcmp(findFileData.cFileName, ".") && strcmp(findFileData.cFileName, ".."))
        {
          ++fileNum;
        }
    } while (FindNextFile(hdle, &findFileData));

_ok: 
    if(INVALID_HANDLE_VALUE != hdle)FindClose(hdle);
    return fileNum;
}

#else//Follwing is for Linux platform
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

int traverse_dir(const char** const dirPath, __hdle *hdle, char* filePath)
{
    DIR* hDir = (DIR*)*hdle;
    const char* fileName = NULL;

    if(!hDir)
    {
        hDir = opendir(*dirPath);
        if(!hDir){
            errorP("Fail to open dir(%s), strerror(%s)\n", *dirPath, strerror(errno));
            return __LINE__;
        }
        *hdle = hDir;
    }

    do{
        struct dirent* dirEntry = NULL;

        dirEntry = readdir(hDir);
        if(!dirEntry){
            debugP("travese end.\n");
            closedir(hDir);
            return __LINE__;
        }
        fileName = dirEntry->d_name;
    }while(!strcmp(".", fileName) || !strcmp("..", fileName));

    sprintf(filePath, "%s/%s", *dirPath, fileName);
    return 0;
}
#endif//#if defined(WIN32)


#ifdef BUILD_DLL
DLL_API
#endif// #ifdef BUILD_DLL
int res_img_unpack(const char* const path_src, const char* const unPackDirPath, int needCheckCrc)
{
  char* itemReadBuf = NULL;
  FILE* fdResImg = NULL;
  AmlResItemHead_t* pItemHead = NULL;
  int ret = 0;
  FILE* fp_item = NULL;

  fdResImg = fopen(path_src, "rb");
  if(!fdResImg){
      errorP("Fail to open res image at path %s\n", path_src);
      return __LINE__;
  }

  itemReadBuf = new char[ITEM_READ_BUF_SZ];
  if(!itemReadBuf){
      errorP("Fail to new buffer at size 0x%x\n", ITEM_READ_BUF_SZ);
      fclose(fdResImg), fdResImg = NULL;
      return __LINE__;
  }

  const unsigned ImgFileSz = get_filesize(path_src);
  if(ImgFileSz <= IMG_HEAD_SZ){
      errorP("file size 0x%x too small\n", ImgFileSz);
      fclose(fdResImg); delete[] itemReadBuf;
      return __LINE__;
  }

  unsigned thisReadSz = IMG_HEAD_SZ + ITEM_HEAD_SZ;
  unsigned actualReadSz = 0;

  actualReadSz = fread(itemReadBuf, 1, thisReadSz, fdResImg);
  if(actualReadSz != thisReadSz){
      errorP("Want to read 0x%x, but only read 0x%x\n", thisReadSz, actualReadSz);
      fclose(fdResImg); delete[] itemReadBuf;
      return __LINE__;
  }

  AmlResImgHead_t* pImgHead = (AmlResImgHead_t*)itemReadBuf;
  pItemHead                 = (AmlResItemHead_t*)(pImgHead + 1);
  const unsigned   totalItemNum = pImgHead->imgItemNum;
  const unsigned orgCrc = pImgHead->crc;
  int isNewVersion = 1;//the resource image is the new version with head

  if(!strncmp(AML_RES_IMG_V1_MAGIC, (char*)pImgHead->magic, AML_RES_IMG_V1_MAGIC_LEN))
  {//new version magic matched
      if(ImgFileSz != pImgHead->imgSz){
          errorP("error, image size in head 0x%x != fileSz 0x%x\n", pImgHead->imgSz, ImgFileSz);
          fclose(fdResImg); delete[] itemReadBuf;
          return __LINE__;
      }
      if(AML_RES_IMG_VERSION != pImgHead->version){
          errorP("Error, version 0x%x not matched\n", pImgHead->version);
          fclose(fdResImg); delete[] itemReadBuf;
          return __LINE__;
      }

      if(needCheckCrc)
      {
          ret = check_img_crc(fdResImg, 4, orgCrc);
          if(ret){
              errorP("Error when check crc\n");
              fclose(fdResImg); delete[] itemReadBuf;
              return __LINE__;
          }
      }
  }
  else
  {
      debugP("magic error, try old version image.\n");
      pItemHead = (AmlResItemHead_t*)pImgHead;
      isNewVersion = 0;
  }

  //for each loop: 
  //    1, read item body and save as file; 
  //    2, get next item head
  unsigned totalReadSz = isNewVersion ? thisReadSz : ITEM_HEAD_SZ;
  const unsigned   itemAlignSz  = isNewVersion ? pImgHead->alignSz : AML_RES_IMG_ITEM_ALIGN_SZ;
  const unsigned   itemAlignMod = itemAlignSz - 1;
  const unsigned   itemSzAlignMask = ~itemAlignMod;
  unsigned totalReadItemNum = 0;

  fseek(fdResImg, totalReadSz, SEEK_SET);//seek from body of the very fist item 
  do{
      const unsigned thisItemBodySz = pItemHead->size; 
      const unsigned thisItemBodyOccupySz =  (thisItemBodySz & itemSzAlignMask) + itemAlignSz;
      const unsigned stuffLen       = thisItemBodyOccupySz - thisItemBodySz;
      char itemFullPath[MAX_PATH*2];//TODO:512 is enough ??
      int hasNextItem = pItemHead->next;

      if(IH_MAGIC != pItemHead->magic){
          errorP("Magic 0x%x != IH_MAGIC 0x%x\n", pItemHead->magic, IH_MAGIC);
          ret = __LINE__; goto _exit;
      }
      sprintf(itemFullPath, "%s/%s", unPackDirPath, pItemHead->name);
      debugP("item %s\n", itemFullPath);

      fp_item = fopen(itemFullPath, "wb");
      if(!fp_item){
        errorP("Fail to create file %s, strerror(%s)\n", itemFullPath, strerror(errno));
        ret = __LINE__; goto _exit;
      }

      for(unsigned itemTotalReadLen = 0; itemTotalReadLen < thisItemBodyOccupySz; )
      {
          const unsigned leftLen = thisItemBodyOccupySz - itemTotalReadLen;

          thisReadSz = min(leftLen, ITEM_READ_BUF_SZ);
          actualReadSz = fread(itemReadBuf, 1, thisReadSz, fdResImg);
          if(thisReadSz != actualReadSz){
              errorP("thisReadSz 0x%x != actualReadSz 0x%x\n", thisReadSz, actualReadSz);
              ret = __LINE__;goto _exit;
          }
          itemTotalReadLen += thisReadSz;

          const unsigned thisWriteSz = itemTotalReadLen < thisItemBodySz ? thisReadSz : (thisReadSz - stuffLen);
          actualReadSz = fwrite(itemReadBuf, 1, thisWriteSz, fp_item);
          if(thisWriteSz != actualReadSz){
              errorP("want write 0x%x, but 0x%x\n", thisWriteSz, actualReadSz);
              ret = __LINE__;goto _exit;
          }

      }
      fclose(fp_item), fp_item = NULL;

      ++totalReadItemNum;
      totalReadSz += thisItemBodyOccupySz;
      if(!hasNextItem)break;//stop parsing

      //get next item head
      actualReadSz = fread(itemReadBuf, 1, ITEM_HEAD_SZ, fdResImg);
      if(actualReadSz != ITEM_HEAD_SZ){
          errorP("Fail to read head, Want to read %d, but %u\n", (int)ITEM_HEAD_SZ, actualReadSz);
          ret = __LINE__; goto _exit;
      }
      pItemHead = (AmlResItemHead_t*)itemReadBuf;
      totalReadSz += ITEM_HEAD_SZ;

  }while(totalReadSz < ImgFileSz);

  
_exit:
  if(fp_item)fclose(fp_item), fp_item;
  if(itemReadBuf)delete[] itemReadBuf, itemReadBuf = NULL;
  if(fdResImg)fclose(fdResImg), fdResImg = NULL;
  return ret;
}

/*
 * 1,
 */
static int _img_pack(const char** const path_src, const char* const packedImg, 
        pFunc_getFile getFile, const int totalFileNum)
{
	FILE *fd_src = NULL;
	FILE *fd_dest = NULL;
	unsigned int pos = 0;
	char file_path[MAX_PATH];
	const char *filename = NULL;
    unsigned imageSz = 0;
    const unsigned BufSz = ITEM_READ_BUF_SZ;
    char* itemBuf = NULL;
    unsigned thisWriteLen = 0;
    unsigned actualWriteLen = 0;
    int ret = 0;
    __hdle hDir = NULL;
    const unsigned   itemAlignSz  = AML_RES_IMG_ITEM_ALIGN_SZ;
    const unsigned   itemAlignMod = itemAlignSz - 1;
    const unsigned   itemSzAlignMask = ~itemAlignMod;
    AmlResImgHead_t* aAmlResImgHead = NULL;

	fd_dest = fopen(packedImg, "wb+");
	if(NULL == fd_dest)	{
		fprintf(stderr,"open %s failed: %s\n", packedImg, strerror(errno));
		return __LINE__;
	}

    itemBuf = new char[BufSz];
    if(!itemBuf){
        errorP("Exception: fail to alloc buuffer\n");
        fclose(fd_dest); return __LINE__;
    }

    thisWriteLen = IMG_HEAD_SZ;
    memset(itemBuf, 0, thisWriteLen);
    actualWriteLen = fwrite(itemBuf, 1, thisWriteLen, fd_dest);
    if(actualWriteLen != thisWriteLen){
        errorP("fail to write head, want 0x%x, but 0x%x\n", thisWriteLen, actualWriteLen);
        fclose(fd_dest); delete[] itemBuf;
        return __LINE__;
    }
    imageSz += IMG_HEAD_SZ; //Increase imageSz after pack each item

    const unsigned totalItemNum = totalFileNum ? totalFileNum : get_dir_filenums(*path_src);
	//for each loop: first create item header and pack it, second pack the item data
    //Fill the item head, 1) magic, 2)data offset, 3)next head start offset
    for(unsigned itemIndex = 0; itemIndex < totalItemNum; ++itemIndex)
    {
        char filePath[MAX_PATH*2];
        
        if(totalFileNum)//File list mode
        {
            if((*getFile)(path_src, (__hdle*)itemIndex, filePath)) {
                break;
            }
        }
        else
        {//file directory mode
          if((*getFile)(path_src, &hDir, filePath)) {
            break;
          }
        }
        const size_t itemSz = get_filesize(filePath);
        const char*  itemName = get_filename(filePath);
        const unsigned itemBodyOccupySz =  (itemSz & itemSzAlignMask) + itemAlignSz;
        const unsigned itemStuffSz      = itemBodyOccupySz - itemSz;
        
        if(IH_NMLEN - 1 < strlen(itemName)){
            errorP("len of item %s is %d > max(%d)\n", itemName, (int)strlen(itemName), IH_NMLEN - 1);
            ret = __LINE__; goto _exit;
        }
        AmlResItemHead_t itemHead;
        memset(&itemHead, 0, IMG_HEAD_SZ);
        itemHead.magic = IH_MAGIC;
        itemHead.size = itemSz;
        imageSz += ITEM_HEAD_SZ;
        itemHead.start = imageSz;
        itemHead.index = itemIndex;
        imageSz   += itemBodyOccupySz;
        itemHead.next  = (itemIndex + 1 == totalItemNum) ? 0 : imageSz;
        itemHead.nums   = totalItemNum;
        memcpy(itemHead.name, itemName, strlen(itemName));
        debugP("pack item [%s]\n", itemName);

        thisWriteLen = ITEM_HEAD_SZ;
        actualWriteLen = fwrite(&itemHead, 1, thisWriteLen, fd_dest);
        if(actualWriteLen != thisWriteLen){
            errorP("Want to write 0x%x, but acutual 0x%x\n", thisWriteLen, actualWriteLen);
            ret = __LINE__; goto _exit;
        }

        fd_src = fopen(filePath, "rb");
        if(!fd_src){
            errorP("Fail to open file [%s], strerror[%s]\n", filePath, strerror(errno));
            ret = __LINE__; goto _exit;
        }
        for(size_t itemWriteLen = 0; itemWriteLen < itemSz; itemWriteLen += thisWriteLen)
        {
            size_t leftLen = itemSz - itemWriteLen;

            thisWriteLen = leftLen > BufSz ? BufSz : leftLen;
            actualWriteLen = fread(itemBuf, 1, thisWriteLen, fd_src);
            if(actualWriteLen != thisWriteLen){
                errorP("Want to read 0x%x but actual 0x%x, at itemWriteLen 0x%x, leftLen 0x%x\n", 
                        thisWriteLen, actualWriteLen, (unsigned)itemWriteLen, (unsigned)leftLen);
                ret = __LINE__; goto _exit;
            }
            actualWriteLen = fwrite(itemBuf, 1, thisWriteLen, fd_dest);
            if(actualWriteLen != thisWriteLen){
                errorP("Want to write 0x%x but actual 0x%x\n", thisWriteLen, actualWriteLen);
                ret = __LINE__; goto _exit;
            }
        }
        fclose(fd_src), fd_src = NULL;
        memset(itemBuf, 0, itemStuffSz);
        thisWriteLen = itemStuffSz;
        actualWriteLen = fwrite(itemBuf, 1, thisWriteLen, fd_dest);
        if(actualWriteLen != thisWriteLen){
            errorP("Want to write 0x%x, but 0x%x\n", thisWriteLen, actualWriteLen);
            ret = __LINE__; goto _exit;
        }
    }


    //Create the header 
    aAmlResImgHead = (AmlResImgHead_t*)itemBuf;
    memset(aAmlResImgHead, 0, sizeof(AmlResImgHead_t));
    aAmlResImgHead->version = AML_RES_IMG_VERSION;
    memcpy(&aAmlResImgHead->magic[0], AML_RES_IMG_V1_MAGIC, AML_RES_IMG_V1_MAGIC_LEN);
    aAmlResImgHead->imgSz       = imageSz;
    aAmlResImgHead->imgItemNum  = totalItemNum;
    aAmlResImgHead->alignSz     = itemAlignSz;
    aAmlResImgHead->crc         = 0;
    //
    //Seek to file header to correct the header
    fseek(fd_dest, 0, SEEK_SET);
    thisWriteLen = IMG_HEAD_SZ;
    actualWriteLen = fwrite(itemBuf, 1, thisWriteLen, fd_dest);
    if(actualWriteLen != thisWriteLen){
        errorP("Want to write 0x%x, but 0x%x\n", thisWriteLen, actualWriteLen);
        ret = __LINE__; goto _exit;
    }

    aAmlResImgHead->crc = calc_img_crc(fd_dest, 4);//Gen crc32
    fseek(fd_dest, 0, SEEK_SET);
    actualWriteLen = fwrite(&aAmlResImgHead->crc, 1, 4, fd_dest);
    if(4 != actualWriteLen){
        errorP("Want to write 4, but %d\n", actualWriteLen);
        ret = __LINE__; goto _exit;
    }

_exit:
    if(itemBuf) delete[] itemBuf, itemBuf = NULL;
    if(fd_src) fclose(fd_src), fd_src = NULL;
	if(fd_dest) fclose(fd_dest), fd_dest = NULL;
	return ret;
}

#ifdef BUILD_DLL
DLL_API
#endif// #ifdef BUILD_DLL
int res_img_pack(const char* szDir, const char* const outResImg)
{
    return _img_pack(&szDir, outResImg, traverse_dir, 0);
}

#ifndef BUILD_DLL 
static const char * const doc = "Amlogic `imgpack v2' usage:\n\
\n\
  # unpack files to the archive\n\
  imgpack -d [archive]  [destination-directory]\n\
  # pack files in directory to the archive\n\
  imgpack -r [source-directory]  [archive]\n\
  # pack files to the archive\n\
  imgpack  [file1].. [fileN]  [archive]\n";

int main(int argc, const char ** const argv)
{
	int ret = 0;
	int c = 0;
    const char* opt = argv[1];

	if(argc < 3) {
		printf("invalid arguments\n");
		printf("%s",doc);
		exit(-1);
	}

    if(!strcmp("?", opt))
    {
        printf("%s",doc);
        exit(-1);
    }
    if(!strcmp("-d", opt))//Unpack imgPath, dest-dir
    {
        ret = res_img_unpack(argv[2], argv[3], 1);
        exit(ret);
    }
    
    if(!strcmp("-r", opt))//pack: fileSrc, destImg, 
    {
        ret = res_img_pack(argv[2], argv[3]);
        exit(ret);
    }

    ret = _img_pack(&argv[1], argv[argc -1], get_file_path_from_argv, argc - 2);
	exit(ret);
}
#endif// #ifndef BUILD_DLL 
