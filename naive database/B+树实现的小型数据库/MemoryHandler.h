#ifndef _MEMORYHANDLER_H_
#define _MEMORYHANDLER_H_

#include"errno.h"
#include"mm.h"
//#include "mman.h"
#include <cstring>
#include <string>
#include "types.h"
#include <sys/stat.h>
#include <fcntl.h> 
#include "unistd.h"
#include<iostream>
using namespace std;

#define PAGESIZE (4096)
#define SHIFT (5)
#define MASK  (0x1F)
#define BITMAPSIZE (128)
#define PAGEREST (PAGESIZE-BITMAPSIZE*4-sizeof(long))


template<typename ValueType>
class MemoryHandler
{
public:
    MemoryHandler(const char* filename);
    long long insert(ValueType* value);
    void remove(long long addr);
    void update(long long addr,ValueType* value);
    void* getAddr(long long addr);
    void unMapAddr(long long addr);
    int compare(long long addr,const ValueType& value);
    ValueType getValue(long long addr);
    long getTotal();
private:
    int fd;
    int valueCapacity;
    char pageInitialize[PAGESIZE];
    MemoryHandler(){};
    struct Header
    {
        long total;
	long valueEmpty;
	int valueSize;
    };
    Header* header;
    struct ValuePage
    {
        int nextEmptyPage;
	int firstEmptyRoom;
	int emptyBitmap[BITMAPSIZE];
        char value[PAGEREST];
        void initialize()
	{
	   nextEmptyPage = 0;
	   firstEmptyRoom=0;
           memset(emptyBitmap,0,BITMAPSIZE*sizeof(int));
        }
	void set(int i)     {        emptyBitmap[i>>SHIFT] |=  (1<<(i&MASK));}

        void clear(int i)   {        emptyBitmap[i>>SHIFT] &= ~(1<<(i&MASK));}

        int test(int i)    { return  emptyBitmap[i>>SHIFT] &   (1<<(i&MASK));}

    };

    struct AddrCache
    {
        AddrCache():next(NULL),prev(NULL){};
        AddrCache* next;
	AddrCache* prev;
	int pageNum;
	int invokeTime;
	char* firstAddr;
    };
    AddrCache* valueCache;
    void addPage()
    {
        write(fd, pageInitialize, PAGESIZE);
        if (header)
            header->total++;
    }

    
};



template<typename ValueType>
MemoryHandler<ValueType>::MemoryHandler(const char* fileName)
{
    if (sizeof(ValueType)>PAGEREST)
        throw string("Memory Handler Error: Value Type Size Too Large!");
    memset(pageInitialize,0,PAGESIZE);
    fd = open(fileName, O_RDWR | O_APPEND, S_IREAD | S_IWRITE);
    if (fd == -1)
    {
        fd = open(fileName, O_RDWR | O_APPEND | O_CREAT, S_IREAD | S_IWRITE);
        if (fd == -1)
            throw string("Memory Handler Error: file open failed!");
        addPage();
    }
    valueCache=NULL;
    header=static_cast<Header*>(mmap(NULL, sizeof(Header), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    if (header->total == 0)
    {
        header->total = 1;
        header->valueEmpty=0;
        header->valueSize = sizeof(ValueType);
        valueCapacity = (PAGEREST) / sizeof(ValueType);
    } 
    else
    {	
        valueCapacity = (PAGEREST) / sizeof(ValueType);
    }
}

template<typename ValueType>
void MemoryHandler<ValueType>::update(long long addr,ValueType* value)
{
    long currentPageIndex=addr>>12;
    int posInPage=addr & ((1<<12)-1);
    int indexInPage=(posInPage-(PAGESIZE-PAGEREST))/header->valueSize;
    ValuePage* currentPage=static_cast<ValuePage*>(mmap(NULL,sizeof(ValuePage),PROT_READ | PROT_WRITE, MAP_SHARED, fd, currentPageIndex<<12));
    char* dest=(char*)(currentPage->value)+indexInPage*header->valueSize;
    memcpy(dest,(char*)value,header->valueSize);
    munmap(currentPage,sizeof(ValuePage));
}

template<typename ValueType>
ValueType MemoryHandler<ValueType>::getValue(long long addr)
{
    long currentPageIndex=addr>>12;
    int posInPage=addr & ((1<<12)-1);
    int indexInPage=(posInPage-(PAGESIZE-PAGEREST))/header->valueSize;
    ValuePage* currentPage=static_cast<ValuePage*>(mmap(NULL,sizeof(ValuePage),PROT_READ | PROT_WRITE, MAP_SHARED, fd, currentPageIndex<<12));
    ValueType dest;
    memcpy(&dest,(char*)(currentPage->value)+(indexInPage)*header->valueSize,header->valueSize);
    munmap(currentPage,sizeof(ValuePage));
    return dest;
}

template<typename ValueType>
long long MemoryHandler<ValueType>::insert(ValueType* value)
{
    AddrCache* tmp;
    tmp=valueCache;
    ValuePage* currentPage;
    bool isCached=false;
    long long indexAddr;
    int BytePos,BitPos;
    if (header->valueEmpty)
    {
        while (tmp)
        {
            if (header->valueEmpty==tmp->pageNum)
    	    {
	        currentPage=(ValuePage*)(tmp->firstAddr-(PAGESIZE-PAGEREST));
  	        isCached=true;
	        break;
	    }
	    tmp=tmp->next;
	}
	if (!isCached)
	 currentPage=(ValuePage*)(mmap(NULL, sizeof(ValuePage), PROT_READ | PROT_WRITE, MAP_SHARED, fd, (header->valueEmpty)<<12));
    }
    else
    {
        addPage();
	currentPage=(ValuePage*)(mmap(NULL,sizeof(ValuePage),PROT_READ | PROT_WRITE, MAP_SHARED, fd, (header->total-1)<<12));
	currentPage->initialize();
	header->valueEmpty=header->total-1;
    }
    indexAddr=(header->valueEmpty)*PAGESIZE+PAGESIZE-PAGEREST+(currentPage->firstEmptyRoom)*header->valueSize;
    char* dest=static_cast<char*>(currentPage->value)+header->valueSize*(currentPage->firstEmptyRoom);
    memcpy(dest,(char*)(value),header->valueSize);
    currentPage->set(currentPage->firstEmptyRoom);
    for (BytePos=(currentPage->firstEmptyRoom)>>3;BytePos<=valueCapacity>>3;BytePos++)
    {
        if (currentPage->emptyBitmap[BytePos]!=0xFF) break;
    }
    for (BitPos=BytePos<<3;BitPos<(BytePos+1)<<3;BitPos++)
    {
        if (BitPos>=valueCapacity)
	{
	    header->valueEmpty=currentPage->nextEmptyPage;
	    currentPage->firstEmptyRoom=-1;
	    break;
	}
	if (!currentPage->test(BitPos))
	{
	    currentPage->firstEmptyRoom=BitPos;
	    break;
	}
    }
    if (!isCached)
        munmap(currentPage,sizeof(ValuePage));
    return indexAddr;

}


template<typename ValueType>
void MemoryHandler<ValueType>::remove(long long addr)
{
    long currentPageIndex=addr>>12;
    int posInPage=addr & ((1<<12)-1);
    int indexInPage=(posInPage-(PAGESIZE-PAGEREST))/header->valueSize;
    ValuePage* currentPage=static_cast<ValuePage*>(mmap(NULL,sizeof(ValuePage),PROT_READ | PROT_WRITE, MAP_SHARED, fd, currentPageIndex<<12));
    char* dest=static_cast<char*>(currentPage->value)+ posInPage;   
    if (currentPage->firstEmptyRoom==-1)
    {
        currentPage->nextEmptyPage=header->valueEmpty;
        header->valueEmpty=currentPageIndex;
    }
    if (currentPage->firstEmptyRoom==-1 || indexInPage<currentPage->firstEmptyRoom)
        currentPage->firstEmptyRoom=indexInPage;
    currentPage->clear(indexInPage);
    munmap(currentPage,sizeof(ValuePage));
}



template<typename ValueType>
void* MemoryHandler<ValueType>::getAddr(long long addr)
{
    long currentPageIndex=addr>>12;
    int posInPage=addr & ((1<<12)-1);
    int indexInPage=(posInPage-(PAGESIZE-PAGEREST))/header->valueSize;
    AddrCache* tmp;
    tmp=valueCache;
    while (tmp)
    {
        if (tmp->pageNum==currentPageIndex)
	{
	    tmp->invokeTime++;
	    return tmp->firstAddr+(indexInPage)*header->valueSize;
	}
	tmp=tmp->next;
    }

    ValuePage* currentPage=static_cast<ValuePage*>(mmap(NULL,sizeof(ValuePage),PROT_READ | PROT_WRITE, MAP_SHARED, fd, currentPageIndex<<12));
    AddrCache* newCache=new AddrCache;
    newCache->pageNum=currentPageIndex;
    newCache->invokeTime=1;
    newCache->firstAddr=currentPage->value;
    if (!valueCache) 
    {
        valueCache=newCache;
    }
    else 
    {
        newCache->prev=valueCache;
        newCache->next=valueCache->next;
	if (newCache->next) newCache->next->prev=newCache;
	valueCache->next=newCache;
    }
    return currentPage->value+(indexInPage)*header->valueSize;
}




template<typename ValueType>
void MemoryHandler<ValueType>::unMapAddr(long long addr)
{
    long currentPageIndex=addr>>12;
    AddrCache* tmp;
    tmp=valueCache;
    while (tmp)
    {
        if (tmp->pageNum==currentPageIndex)
	{
	    tmp->invokeTime--;
	    if (tmp->invokeTime==0)
	    {
	        munmap((ValuePage*)(tmp->firstAddr-(PAGESIZE-PAGEREST)),sizeof(ValuePage));
		if (tmp->prev && tmp->next)
		{
		    tmp->prev->next=tmp->next;
		    tmp->next->prev=tmp->prev;
		    delete tmp;
		}
		else if (tmp->prev)
		{
		    tmp->prev->next=NULL;
		    tmp->prev=NULL;
		    delete tmp;
		}
		else if (tmp->next)
		{
		    tmp->next->prev=NULL;
		    valueCache=tmp->next;
		    tmp->next=NULL;
		    delete tmp;
		}
		else
		{
		    delete tmp;
		    valueCache=NULL;
		}
		break;
	    }
	}
	tmp=tmp->next;
    }
}



template<typename ValueType>
long MemoryHandler<ValueType>::getTotal()
{
    return header->total;
}



template<typename ValueType>
int MemoryHandler<ValueType>::compare(long long addr,const ValueType& value)
{
    long currentPageIndex=addr>>12;
    int posInPage=addr & ((1<<12)-1);
    int indexInPage=(posInPage-(PAGESIZE-PAGEREST))/header->valueSize;
    ValuePage* currentPage=static_cast<ValuePage*>(mmap(NULL,sizeof(ValuePage),PROT_READ | PROT_WRITE, MAP_SHARED, fd, currentPageIndex<<12));
    ValueType dest;
    memcpy(&dest,(char*)(currentPage->value)+(indexInPage)*header->valueSize,header->valueSize);
    munmap(currentPage,sizeof(ValuePage));
    if (dest==value) return 0;
    else if (dest<value) return -1;
    else return 1;
}


#endif
