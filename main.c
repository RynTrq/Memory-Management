/*
All the main functions with respect to the MeMS are inplemented here
read the function discription for more details

NOTE: DO NOT CHANGE THE NAME OR SIGNATURE OF FUNCTIONS ALREADY PROVIDED
you are only allowed to implement the functions 
you can also make additional helper functions a you wish

REFER DOCUMENTATION FOR MORE DETAILS ON FUNSTIONS AND THEIR FUNCTIONALITY
*/
// add other headers as required
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <inttypes.h>

#define PAGE_SIZE 4096;

typedef struct Addresses{
    int size;
    void* address;
    struct Addresses* next;
    struct Addresses* previous;
}Address;

typedef struct spaceReturnType {
    Address adr;
    int offset;
    int searchCode;
}spaceReturnType;

typedef struct MemTracker {
    Address addressVariable;
    struct MemTracker* next;
    struct MemTracker* previous;
    int pagesDone;
}MemTracker;

typedef struct filledAddress {
    void* address;
    struct filledAddress* next;
    struct filledAddress* previous;
    int noOfPages;
}filledAddress;

typedef struct filledSpaceReturnType {
    Address adr;
    MemTracker memAdr;
    int searchCode;
}filledSpaceReturnType;

/*These are two lists which indicates the starting of the two linked lists that I've made. The MemTracker and the Address.*/
MemTracker* freeList = NULL;
filledAddress* filledList = NULL;


/*
Use this macro where ever you need PAGE_SIZE.
As PAGESIZE can differ system to system we should have flexibility to modify this 
macro to make the output of all system same and conduct a fair evaluation. 
*/
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif


/*
Initializes all the required parameters for the MeMS system. The main parameters to be initialized are:
1. the head of the free list i.e. the pointer that points to the head of the free list
2. the starting MeMS virtual address from which the heap in our MeMS virtual address space will start.
3. any other global variable that you want for the MeMS implementation can be initialized here.
Input Parameter: Nothing
Returns: Nothing
*/

void mems_init(){
    
    MemTracker* freeList = NULL;
    filledAddress* filledList = NULL;
}

/*
This function will be called at the end of the MeMS system and its main job is to unmap the 
allocated memory using the munmap system call.
Input Parameter: Nothing
Returns: Nothing
*/

filledAddress findLastFilled(filledAddress currentAdr){
    if(currentAdr.next==NULL){
        return currentAdr;
    }else{
        return findLastFilled(*currentAdr.next);
    }
}

Address* searchInsideAdr(void* ptr, Address* currentAdress){
    Address currentAdr = *currentAdress;
    if(currentAdr.next==NULL){
        if(currentAdr.address==ptr){
            return currentAdress;
        }else{
            return NULL;
        }
    }else{
        if(currentAdr.address==NULL){
            return searchInsideAdr(ptr, currentAdr.next);
        }else{
            if(currentAdr.address==ptr){
                return currentAdress;
            }else{
                return searchInsideAdr(ptr, currentAdr.next);
            }
        }
    }
}


filledSpaceReturnType searchInsideMem(void* ptr, MemTracker currentMem){     //
    if(currentMem.pagesDone==0){
        if(currentMem.next==NULL){
            if(currentMem.addressVariable.address==ptr){
                filledSpaceReturnType toBeReturned;
                toBeReturned.adr = currentMem.addressVariable;
                toBeReturned.searchCode = 0;
                toBeReturned.memAdr = currentMem;
                return toBeReturned;
            }
            Address* searchResultAddress = searchInsideAdr(ptr, &(currentMem.addressVariable));
            if(searchResultAddress==NULL){
                filledSpaceReturnType toBeReturned;
                toBeReturned.searchCode = -1;
                return toBeReturned;
            }else{
                filledSpaceReturnType toBeReturned;
                toBeReturned.adr = *searchResultAddress;
                toBeReturned.searchCode = 1;
                toBeReturned.memAdr = currentMem;
                return toBeReturned;
            }
        }else{
            if(currentMem.addressVariable.address==ptr){
                filledSpaceReturnType toBeReturned;
                toBeReturned.adr = currentMem.addressVariable;
                toBeReturned.searchCode = 0;
                toBeReturned.memAdr = currentMem;
                return toBeReturned;
            }
            Address* searchResultAddress = searchInsideAdr(ptr, &(currentMem.addressVariable));
            if(searchResultAddress==NULL){
                return searchInsideMem(ptr, *currentMem.next);
            }else{
                filledSpaceReturnType toBeReturned;
                toBeReturned.adr = *searchResultAddress;
                toBeReturned.searchCode = 1;
                toBeReturned.memAdr = currentMem;
                return toBeReturned;
            }
        }
    }else{
        uintptr_t addressVal = (uintptr_t)currentMem.addressVariable.address - (4096 * currentMem.pagesDone);
        if(addressVal==(uintptr_t)ptr){
            filledSpaceReturnType toBeReturned;
            toBeReturned.adr = currentMem.addressVariable;
            toBeReturned.searchCode = 0;
            toBeReturned.memAdr = currentMem;
            return toBeReturned;
        }else{
            if(currentMem.next==NULL){
                Address* searchResultAddress = searchInsideAdr(ptr, &(currentMem.addressVariable));
                if(searchResultAddress==NULL){
                    filledSpaceReturnType toBeReturned;
                    toBeReturned.searchCode = -1;
                    return toBeReturned;
                }else{
                    filledSpaceReturnType toBeReturned;
                    toBeReturned.adr = *searchResultAddress;
                    toBeReturned.searchCode = 1;
                    toBeReturned.memAdr = currentMem;
                    return toBeReturned;
                }
            }else{
                Address* searchResultAddress = searchInsideAdr(ptr, &(currentMem.addressVariable));
                if(searchResultAddress==NULL){
                    return searchInsideMem(ptr, *currentMem.next);
                }else{
                    filledSpaceReturnType toBeReturned;
                    toBeReturned.adr = *searchResultAddress;
                    toBeReturned.searchCode = 1;
                    toBeReturned.memAdr = currentMem;
                    return toBeReturned;
                }
            }
        }
    }
}


spaceReturnType searchForSpace(Address inputAddress, int searchSize, int remainingSpace){
    if(inputAddress.next == NULL){
        remainingSpace -= inputAddress.size;
        if(remainingSpace>=searchSize){
            spaceReturnType toBeReturned;
            toBeReturned.adr = inputAddress;
            toBeReturned.offset = 4096 - remainingSpace;
            toBeReturned.searchCode = 1;
            return toBeReturned;
        }else{
            spaceReturnType toBeReturned;
            toBeReturned.searchCode = 0;
        }
    }else{
        if(inputAddress.address == NULL){
            if(inputAddress.size>=searchSize){
                spaceReturnType toBeReturned;
                toBeReturned.adr = inputAddress;
                toBeReturned.offset = 4096 - remainingSpace;
                toBeReturned.searchCode = 2;
                return toBeReturned;
            }else{
                return searchForSpace(*inputAddress.next, searchSize, remainingSpace - inputAddress.size);
            }
        }else{
            return searchForSpace(*inputAddress.next, searchSize, remainingSpace - inputAddress.size);
        }
    }
}


void* allocations(MemTracker inputMem,int size){
    if(inputMem.next==NULL){
        spaceReturnType searchResult = searchForSpace(inputMem.addressVariable,size,4096);
        if(searchResult.searchCode==0){
            return NULL;
        }else if(searchResult.searchCode==1){
            Address track;
            void* newAdr = mmap(inputMem.addressVariable.address,size, PROT_READ | PROT_WRITE, MAP_PRIVATE, -1, searchResult.offset);
            searchResult.adr.next = &track;
            track.previous = &searchResult.adr;
            track.address = newAdr;
            track.size = size;
            return newAdr;
        }else{
            void* newAdr = mmap(inputMem.addressVariable.address,size, PROT_READ | PROT_WRITE, MAP_PRIVATE, -1, searchResult.offset);
            if(searchResult.adr.size == size){
                searchResult.adr.address = newAdr;
                return newAdr;
            }else{
                Address track;
                track.address = NULL;
                track.size = searchResult.adr.size - size ;
                track.next = searchResult.adr.next;
                track.previous = &searchResult.adr;
                searchResult.adr.address = newAdr;
                searchResult.adr.size = size;
                searchResult.adr.next = &track;
                return newAdr;
            }
        }
    }else{
        spaceReturnType searchResult = searchForSpace(inputMem.addressVariable,size,4096);
        if(searchResult.searchCode==0){
            return allocations(*inputMem.next,size);
        }else if(searchResult.searchCode==1){
            Address track;
            void* newAdr = mmap(inputMem.addressVariable.address,size, PROT_READ | PROT_WRITE, MAP_PRIVATE, -1, searchResult.offset);
            searchResult.adr.next = &track;
            track.previous = &searchResult.adr;
            track.address = newAdr;
            track.size = size;
            return newAdr;
        }else{
            void* newAdr = mmap(inputMem.addressVariable.address,size, PROT_READ | PROT_WRITE, MAP_PRIVATE, -1, searchResult.offset);
            if(searchResult.adr.size == size){
                searchResult.adr.address = newAdr;
                return newAdr;
            }else{
                Address track;
                track.address = NULL;
                track.size = searchResult.adr.size - size ;
                track.next = searchResult.adr.next;
                track.previous = &searchResult.adr;
                searchResult.adr.address = newAdr;
                searchResult.adr.size = size;
                searchResult.adr.next = &track;
                return newAdr;
            }
        }
    }
}


MemTracker searchLastNull(MemTracker inputMem){
    if(inputMem.next==NULL){
        return inputMem;
    }else{
        return searchLastNull(*(inputMem.next));
    }
}


filledAddress* searchForFilledPage(filledAddress* inputAddress,void* ptr){
    filledAddress inputAdr = *inputAddress;
    if(inputAdr.next==NULL){
        if(inputAdr.address==ptr){
            return inputAddress;
        }else{
            return NULL;
        }
    }else{
        if(inputAdr.address==ptr){
            return inputAddress;
        }else{
            searchForFilledPage(inputAdr.next,ptr);
        }
    }
}


void* myAlloc(int size){
    void* toBeReturned;
    if(size%4096==0){
        toBeReturned = mmap(NULL,size, PROT_READ | PROT_WRITE, MAP_PRIVATE, -1, 0);
        if(filledList==NULL){
            filledAddress newAllocation;
            newAllocation.next=NULL;
            newAllocation.previous=NULL;
            newAllocation.address=toBeReturned;
            newAllocation.noOfPages=size/4096;
            filledList = &newAllocation;
            return toBeReturned;
        }else{
            filledAddress newAllocation;
            filledAddress LastAddress = findLastFilled(*filledList);
            newAllocation.next=NULL;
            newAllocation.previous=&LastAddress;
            newAllocation.address=toBeReturned;
            newAllocation.noOfPages=size/4096;
            return toBeReturned;
        }
    }else{
        if(freeList==NULL){
            MemTracker track;
            track.addressVariable.size = size%4096;
            track.addressVariable.address = mmap(NULL,size, PROT_READ | PROT_WRITE, MAP_PRIVATE, -1, 0);
            track.addressVariable.address += 4096*(size/4096);
            track.addressVariable.previous = NULL;
            track.pagesDone = size/4096;
            track.addressVariable.next = NULL;
            track.next = NULL;
            track.previous = NULL;
            freeList = &track;
            return track.addressVariable.address - 4096*(size/4096);
        }else{
            if(size>4096){
                MemTracker track;
                track.next = NULL;
                track.pagesDone = size/4096;
                track.addressVariable.address = mmap(NULL,size, PROT_READ | PROT_WRITE, MAP_PRIVATE, -1, 0);
                track.addressVariable.address += 4096*(size/4096);
                track.addressVariable.previous = NULL;
                track.addressVariable.size = size%4096;
                track.addressVariable.next = NULL;
                MemTracker lastTrack = searchLastNull(*freeList);
                lastTrack.next = &track;
                track.previous = &lastTrack;
                return track.addressVariable.address - 4096*(size/4096);
            }else{
                void* toBeReturned = allocations(*freeList, size);
                if(toBeReturned == NULL){
                    MemTracker track;
                    track.next = NULL;
                    track.pagesDone = size/4096;
                    track.addressVariable.address = mmap(NULL,size, PROT_READ | PROT_WRITE, MAP_PRIVATE, -1, 0);
                    track.addressVariable.address += 4096*(size/4096);
                    track.addressVariable.previous = NULL;
                    track.addressVariable.size = size%4096;
                    track.addressVariable.next = NULL;
                    MemTracker lastTrack = searchLastNull(*freeList);
                    lastTrack.next = &track;
                    track.previous = &lastTrack;
                    return track.addressVariable.address - 4096*(size/4096);
                }else{
                    return toBeReturned;
                }
            }
        }
    }
}


int myFree(void* ptr){
    if(filledList==NULL){
        if(freeList==NULL){
            return -1;
        }else{
            filledSpaceReturnType searchResult = searchInsideMem(ptr,*freeList);
            if(searchResult.searchCode==-1){
                return -1;
            }else if(searchResult.searchCode==0){
                MemTracker currentmem = searchResult.memAdr;
                if(currentmem.addressVariable.next==NULL){
                    if(currentmem.next==NULL){
                        if(currentmem.previous!=NULL){
                            MemTracker prevMem = *(currentmem.previous);
                            prevMem.next = NULL;
                            munmap(ptr,(((currentmem.pagesDone)*4096)+currentmem.addressVariable.size));
                            return 0;
                        }else{
                            freeList = NULL;
                            munmap(ptr,(((currentmem.pagesDone)*4096)+currentmem.addressVariable.size));
                            return 0;
                        }
                    }else{
                        if(currentmem.previous==NULL){
                            MemTracker nextMem = *(currentmem.next);
                            freeList = &nextMem;
                            nextMem.previous = NULL;
                            munmap(ptr,(((currentmem.pagesDone)*4096)+currentmem.addressVariable.size));
                            return 0;
                        }else{
                            MemTracker nextMem = *(currentmem.next);
                            MemTracker prevMem = *(currentmem.previous);
                            nextMem.previous = &prevMem;
                            prevMem.next = &nextMem;
                            munmap(ptr,(((currentmem.pagesDone)*4096)+currentmem.addressVariable.size));
                            return 0;
                        }
                    }
                }else{
                    Address nextAdd = *(currentmem.addressVariable.next);
                    if(nextAdd.address==NULL){
                        Address superNextAdd = *(nextAdd.next);
                        currentmem.addressVariable = superNextAdd;
                        superNextAdd.previous = NULL;
                        munmap(ptr,(((currentmem.pagesDone)*4096)+currentmem.addressVariable.size));
                        return 0;
                    }else{
                        currentmem.addressVariable = nextAdd;
                        nextAdd.previous = NULL;
                        munmap(ptr,(((currentmem.pagesDone)*4096)+currentmem.addressVariable.size));
                        return 0;
                    }
                }
            }else if(searchResult.searchCode == 1){
                Address currentAdr = searchResult.adr;
                if(currentAdr.next == NULL){
                    Address prevAdr = *(currentAdr.previous);
                    if(prevAdr.address==NULL){
                        Address superPrevAdr = *(prevAdr.previous);
                        superPrevAdr.next = NULL;
                        munmap(ptr, currentAdr.size);
                        return 0;
                    }else{
                        prevAdr.next = NULL;
                        munmap(ptr,currentAdr.size);
                        return 0;
                    }
                }else{
                    Address prevAdr = *(currentAdr.previous);
                    Address nextAdr = *(currentAdr.next);
                    if(prevAdr.address == NULL && nextAdr.address == NULL){
                        Address superNextAdr = *(prevAdr.next);
                        prevAdr.next = &superNextAdr;
                        prevAdr.size = ( prevAdr.size + currentAdr.size + nextAdr.size );
                        superNextAdr.previous = &prevAdr;
                        munmap(ptr,currentAdr.size);
                        return 0;
                    }else if(prevAdr.address == NULL && nextAdr.address!=NULL){
                        prevAdr.next = &nextAdr;
                        nextAdr.previous = &prevAdr;
                        prevAdr.size = ( prevAdr.size + currentAdr.size );
                        munmap(ptr,currentAdr.size);
                        return 0;
                    }else if(prevAdr.address!=NULL && nextAdr.address==NULL){
                        nextAdr.previous = &prevAdr;
                        prevAdr.next = &nextAdr;
                        nextAdr.size = ( currentAdr.size + nextAdr.size );
                        munmap(ptr,currentAdr.size);
                        return 0;
                    }else{
                        currentAdr.address = NULL;
                        munmap(ptr,currentAdr.size);
                        return 0;
                    }
                }
            }
        }
    }else{
        filledAddress* searchResult = searchForFilledPage(filledList,ptr);
        if(searchResult!=NULL){
            filledAddress currentFilledAdr = *(searchResult);
            if(currentFilledAdr.previous == NULL){
                if(currentFilledAdr.next == NULL){
                    filledList = NULL;
                    munmap(ptr,(currentFilledAdr.noOfPages)*40960);
                    return 0;
                }else{
                    filledList = currentFilledAdr.next;
                    filledAddress nextFilledAdr = *(currentFilledAdr.next);
                    nextFilledAdr.previous = NULL;
                    munmap(ptr,(currentFilledAdr.noOfPages)*40960);
                    return 0;
                }
            }else{
                if(currentFilledAdr.next == NULL){
                    filledAddress prevFilledAdr = *(currentFilledAdr.previous);
                    prevFilledAdr.next = NULL;
                    munmap(ptr,(currentFilledAdr.noOfPages)*40960);
                    return 0;
                }else{
                    filledAddress prevFiledAdr = *(currentFilledAdr.previous);
                    filledAddress nextFilledAdr = *(currentFilledAdr.next);
                    prevFiledAdr.next = &nextFilledAdr;
                    nextFilledAdr.previous = &prevFiledAdr;
                    munmap(ptr,(currentFilledAdr.noOfPages)*40960);
                    return 0;
                }
            }
        }else{
            if(freeList==NULL){
                return -1;
            }else{
                filledSpaceReturnType searchResult = searchInsideMem(ptr,*freeList);
                if(searchResult.searchCode==-1){
                    return -1;
                }else if(searchResult.searchCode==0){
                    MemTracker currentmem = searchResult.memAdr;
                    if(currentmem.addressVariable.next==NULL){
                        if(currentmem.next==NULL){
                            if(currentmem.previous!=NULL){
                                MemTracker prevMem = *(currentmem.previous);
                                prevMem.next = NULL;
                                munmap(ptr,(((currentmem.pagesDone)*4096)+currentmem.addressVariable.size));
                                return 0;
                            }else{
                                freeList = NULL;
                                munmap(ptr,(((currentmem.pagesDone)*4096)+currentmem.addressVariable.size));
                                return 0;
                            }
                        }else{
                            if(currentmem.previous==NULL){
                                MemTracker nextMem = *(currentmem.next);
                                freeList = &nextMem;
                                nextMem.previous = NULL;
                                munmap(ptr,(((currentmem.pagesDone)*4096)+currentmem.addressVariable.size));
                                return 0;
                            }else{
                                MemTracker nextMem = *(currentmem.next);
                                MemTracker prevMem = *(currentmem.previous);
                                nextMem.previous = &prevMem;
                                prevMem.next = &nextMem;
                                munmap(ptr,(((currentmem.pagesDone)*4096)+currentmem.addressVariable.size));
                                return 0;
                            }
                        }
                    }else{
                        Address nextAdd = *(currentmem.addressVariable.next);
                        if(nextAdd.address==NULL){
                            Address superNextAdd = *(nextAdd.next);
                            currentmem.addressVariable = superNextAdd;
                            superNextAdd.previous = NULL;
                            munmap(ptr,(((currentmem.pagesDone)*4096)+currentmem.addressVariable.size));
                            return 0;
                        }else{
                            currentmem.addressVariable = nextAdd;
                            nextAdd.previous = NULL;
                            munmap(ptr,(((currentmem.pagesDone)*4096)+currentmem.addressVariable.size));
                            return 0;
                        }
                    }
                }else if(searchResult.searchCode == 1){
                    Address currentAdr = searchResult.adr;
                    if(currentAdr.next == NULL){
                        Address prevAdr = *(currentAdr.previous);
                        if(prevAdr.address==NULL){
                            Address superPrevAdr = *(prevAdr.previous);
                            superPrevAdr.next = NULL;
                            munmap(ptr, currentAdr.size);
                            return 0;
                        }else{
                            prevAdr.next = NULL;
                            munmap(ptr,currentAdr.size);
                            return 0;
                        }
                    }else{
                        Address prevAdr = *(currentAdr.previous);
                        Address nextAdr = *(currentAdr.next);
                        if(prevAdr.address == NULL && nextAdr.address == NULL){
                            Address superNextAdr = *(prevAdr.next);
                            prevAdr.next = &superNextAdr;
                            prevAdr.size = ( prevAdr.size + currentAdr.size + nextAdr.size );
                            superNextAdr.previous = &prevAdr;
                            munmap(ptr,currentAdr.size);
                            return 0;
                        }else if(prevAdr.address == NULL && nextAdr.address!=NULL){
                            prevAdr.next = &nextAdr;
                            nextAdr.previous = &prevAdr;
                            prevAdr.size = ( prevAdr.size + currentAdr.size );
                            munmap(ptr,currentAdr.size);
                            return 0;
                        }else if(prevAdr.address!=NULL && nextAdr.address==NULL){
                            nextAdr.previous = &prevAdr;
                            prevAdr.next = &nextAdr;
                            nextAdr.size = ( currentAdr.size + nextAdr.size );
                            munmap(ptr,currentAdr.size);
                            return 0;
                        }else{
                            currentAdr.address = NULL;
                            munmap(ptr,currentAdr.size);
                            return 0;
                        }
                   }
                }
            }
        }
    }
}

void deallocateAllInsideAddress(Address* currentAddress){
    Address currentAdr = *currentAddress;
    if(currentAdr.next==NULL){
        if(currentAdr.address!=NULL){
            myFree(currentAdr.address);
        }
    }else{
        if(currentAdr.address!=NULL){
            myFree(currentAdr.address);
        }
        deallocateAllInsideAddress(currentAdr.next);
    }
}
void deallocateAllInsideMem(MemTracker* currentMem){
    MemTracker currentMemVar = *currentMem;
    if(currentMemVar.next==NULL){
        deallocateAllInsideAddress(&(currentMemVar.addressVariable));
    }else{
        deallocateAllInsideAddress(&(currentMemVar.addressVariable));
        deallocateAllInsideMem(currentMemVar.next);
    }
}
void deallocateAllInsideFilled(filledAddress* currentFilled){
    filledAddress currentFilledVar = *currentFilled;
    if(currentFilledVar.next==NULL){
        myFree(currentFilledVar.address);
    }else{
        myFree(currentFilledVar.address);
        deallocateAllInsideFilled(currentFilledVar.next);
    }
}


//mem_finish!!
//Remove, ie deaaloate all the memory using munmap().....
void mems_finish(){
    if(freeList==NULL){
        if(filledList==NULL){
            return;
        }else{
            deallocateAllInsideFilled(filledList);
            return;
        }
    }else{
        if(filledList==NULL){
            deallocateAllInsideMem(freeList);
            return;
        }else{
            deallocateAllInsideMem(freeList);
            deallocateAllInsideFilled(filledList);
            return;
        }
    }
}


/*
Allocates memory of the specified size by reusing a segment from the free list if 
a sufficiently large segment is available. 

Else, uses the mmap system call to allocate more memory on the heap and updates 
the free list accordingly.

Note that while mapping using mmap do not forget to reuse the unused space from mapping
by adding it to the free list.
Parameter: The size of the memory the user program wants
Returns: MeMS Virtual address (that is created by MeMS)
*/ 

void* mems_malloc(size_t size)
{
    myAlloc(size);
}

/*
this function print the stats of the MeMS system like
1. How many pages are utilised by using the mems_malloc
2. how much memory is unused i.e. the memory that is in freelist and is not used.
3. It also prints details about each node in the main chain and each segment (PROCESS or HOLE) in the sub-chain.
Parameter: Nothing
Returns: Nothing but should print the necessary information on STDOUT
*/

int countFilledPages(filledAddress* currentAdr){
    int count = 0;
    while(currentAdr != NULL){
        count += currentAdr->noOfPages;
        currentAdr = currentAdr->next;
    }
    return count;
}

int countPages(MemTracker* currentMem){
    int count = 0;
    while(currentMem != NULL){
        count += currentMem->pagesDone;
        currentMem = currentMem->next;
    }
    return count;
}

void mems_print_stats(){
    if(freeList==NULL){
        if(filledList==NULL){
            printf("No memory is allocated\n");
            return;
        }else{
            int noOfFilledPages = countFilledPages(filledList);
            printf("Total number of pages allocated: %d\n",noOfFilledPages);
            printf("Total number of pages free: 0\n");
            printf("Total number of pages used: %d\n",noOfFilledPages);
            printf("Total number of segments: 1\n");
            printf("Total number of holes: 0\n");
            printf("Total number of segments in use: 1\n");
            printf("Total number of holes in use: 0\n");
            printf("Total number of segments not in use: 0\n");
            printf("Total number of holes not in use: 0\n");
            printf("Total number of bytes allocated: %d\n",noOfFilledPages*4096);
            printf("Total number of bytes free: 0\n");
            printf("Total number of bytes used: %d\n",noOfFilledPages*4096);
            printf("Total number of bytes in segments: %d\n",noOfFilledPages*4096);
            printf("Total number of bytes in holes: 0\n");
            printf("Total number of bytes in segments in use: %d\n",noOfFilledPages*4096);
            printf("Total number of bytes in holes in use: 0\n");
            printf("Total number of bytes in segments not in use: 0\n");
            printf("Total number of bytes in holes not in use: 0\n");
            return;
        }
    }else{
        if(filledList==NULL){
            int noOfPages = countPages(freeList);
            printf("Total number of pages allocated: %d\n",noOfPages);
            printf("Total number of pages free: %d\n",noOfPages);
            printf("Total number of pages used: 0\n");
            printf("Total number of segments: 1\n");
            printf("Total number of holes: 0\n");
            printf("Total number of segments in use: 0\n");
            printf("Total number of holes in use: 0\n");
            printf("Total number of segments not in use: 1\n");
            printf("Total number of holes not in use: 0\n");
            printf("Total number of bytes allocated: %d\n",noOfPages*4096);
            printf("Total number of bytes free: %d\n",noOfPages*4096);
            printf("Total number of bytes used: 0\n");
            printf("Total number of bytes in segments: %d\n",noOfPages*4096);
            printf("Total number of bytes in holes: 0\n");
            printf("Total number of bytes in segments in use: 0\n");
            printf("Total number of bytes in holes in use: 0\n");
            printf("Total number of bytes in segments not in use: %d\n",noOfPages*4096);
            printf("Total number of bytes in holes not in use: 0\n");
            return;
        }else{
            int noOfPages = countPages(freeList);
            int noOfFilledPages = countFilledPages(filledList);
            printf("Total number of pages allocated: %d\n",noOfPages+noOfFilledPages);
            printf("Total number of pages free: %d\n",noOfPages);
            printf("Total number of pages used: %d\n",noOfFilledPages);
            printf("Total number of segments: 2\n");
            printf("Total number of holes: 0\n");
            printf("Total number of segments in use: 1\n");
            printf("Total number of holes in use: 0\n");
            printf("Total number of segments not in use: 1\n");
            printf("Total number of holes not in use: 0\n");
            printf("Total number of bytes allocated: %d\n",(noOfPages*4096)+(noOfFilledPages*4096));
            printf("Total number of bytes free: %d\n",noOfPages*4096);
            printf("Total number of bytes used: %d\n",noOfFilledPages*4096);
            printf("Total number of bytes in segments: %d\n",noOfPages*4096+noOfFilledPages*4096);
            printf("Total number of bytes in holes: 0\n");
            printf("Total number of bytes in segments in use: %d\n",noOfFilledPages);
            printf("Total number of bytes in holes in use: 0\n");
            printf("Total number of bytes in segments not in use: %d\n",noOfPages*4096);
            printf("Total number of bytes in holes not in use: 0\n");
            return;
        }
    }
}

/*
Returns the MeMS physical address mapped to ptr ( ptr is MeMS virtual address).
Parameter: MeMS Virtual address (that is created by MeMS)
Returns: MeMS physical address mapped to the passed ptr (MeMS virtual address).
*/

void *mems_get(void*v_ptr){
    filledSpaceReturnType searchResult = searchInsideMem(v_ptr,*freeList);
    if(searchResult.searchCode==-1){
        return NULL;
    }else if(searchResult.searchCode==0){
        return searchResult.adr.address;
    }else{
        return searchResult.adr.address;
    }
}


/*
this function free up the memory pointed by our virtual_address and add it to the free list
Parameter: MeMS Virtual address (that is created by MeMS) 
Returns: nothing
*/
void mems_free(void *v_ptr){
    myFree(v_ptr);
}

int main(){
    mems_init();
    void* ptr1 = mems_malloc(1000);
    void* ptr2 = mems_malloc(2000);
    void* ptr3 = mems_malloc(3000);
    void* ptr4 = mems_malloc(4000);
    void* ptr5 = mems_malloc(5096);
    void* ptr6 = mems_malloc(6096);
    void* ptr7 = mems_malloc(7096);
    void* ptr8 = mems_malloc(8096);
    void* ptr9 = mems_malloc(9192);
    void* ptr10 = mems_malloc(10192);
    void* ptr11 = mems_malloc(4096);
    void* ptr12 = mems_malloc(8192);
    mems_free(ptr11);
    mems_print_stats();
}