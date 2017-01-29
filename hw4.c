/*
*
*
* Kunal Shah
* HW #4 - Garbage Collector
*
* Note: There are a ton of comments everthwere containing printfs that were created for testing
*
* Mark_Region_and_Walk, Sweep, and isPointer are based loosely upon the code algorithims in the book
*
*
*/


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

struct memory_region{
  size_t * start;
  size_t * end;
};

struct memory_region global_mem;
struct memory_region heap_mem;
struct memory_region stack_mem;

void walk_region_and_mark(void* start, void* end);
size_t * is_pointer(size_t * ptr);

//how many ptrs into the heap we have
#define INDEX_SIZE 1000
void* heapindex[INDEX_SIZE];



//------------------------------------**BEGIN FUNCTIONS**------------------------------------//



//grabbing the address and size of the global memory region from proc 
void init_global_range()
{
  char file[100];
  char * line=NULL;
  size_t n=0;
  size_t read_bytes=0;
  size_t start, end;

  sprintf(file, "/proc/%d/maps", getpid());
  FILE * mapfile  = fopen(file, "r");
  if (mapfile==NULL){
    perror("opening maps file failed\n");
    exit(-1);
  }

  int counter=0;
  while ((read_bytes = getline(&line, &n, mapfile)) != -1) {
    if (strstr(line, "hw4")!=NULL){
      ++counter;
      if (counter==3){
        sscanf(line, "%lx-%lx", &start, &end);
        global_mem.start=(size_t*)start;
        // with a regular address space, our globals spill over into the heap
        global_mem.end=malloc(256);
        free(global_mem.end);
      }
    }
    else if (read_bytes > 0 && counter==3) {
      if(strstr(line,"heap")==NULL) {
        // with a randomized address space, our globals spill over into an unnamed segment directly following the globals
        sscanf(line, "%lx-%lx", &start, &end);
        printf("found an extra segment, ending at %zx\n",end);            
        global_mem.end=(size_t*)end;
      }
      break;
    }
  }
  fclose(mapfile);
}



//------------------------------------**MARKING RELATED FUNCTIONS**------------------------------------//



/*  
*  
*  SECOND BIT SIGNIFIES IF THE CHUNK IS MARKED OR NOT
*  1 = Marked ------> If the pointer is null, mark it as 0, therefore free it in sweep
*  0 = Not Marked ------If the pointer is not null, mark it as 1, therefore no need to free it
*
*/

int is_marked(size_t* chunk) 
{
  return ((*chunk) & 0x2) > 0;
}

void mark(size_t* chunk) 
{
  (*chunk)|=0x2;
}

void clear_mark(size_t* chunk) 
{
  (*chunk)&=(~0x2);
}

// chunk related operations

#define chunk_size(c)  ((*((size_t*)c))& ~(size_t)3 ) 
void* next_chunk(void* c) 
{ 
  if(chunk_size(c) == 0) 
  {
    printf("Panic, chunk is of zero size.\n");
  }
  if((c+chunk_size(c)) < sbrk(0))
  {
    return ((void*)c+chunk_size(c));
  }
  else 
  {
    //printf("RETURNING 0 FROM CHUNK SIZE!!!!!!!!!!!!!!!!!\n");
    return 0;
  }
}
int in_use(void *c) 
{ 
  return (next_chunk(c) && ((*(size_t*)next_chunk(c)) & 1));
}


//------------------------------------**HELPER FUNCTIONS**------------------------------------//



//------------------------------------**TODO FUNCTIONS**------------------------------------//

//Most likely not going to do...
#define IND_INTERVAL ((sbrk(0) - (void*)(heap_mem.start - 1)) / INDEX_SIZE)
void build_heap_index() 
{
  size_t* currentHeader = heap_mem.start -1;
  size_t* nextHeader;
  int i;
  for(i=0;i <INDEX_SIZE && currentHeader && currentHeader<heap_mem.end;i++)
  {
      heapindex[i] == currentHeader;
      currentHeader = next_chunk(currentHeader);
  }
  for(;i<INDEX_SIZE;i++)
  {
    heapindex[i] =NULL;
  }


}



/* SWEEP: 
*
* Iterate through every chunk/block in the heap and frees any unmarked allocated blocks
* However, when we free, the heap size changes thus we need to adjust heap_mem.end or just compare to sbrk(0)
*
*/
void sweep() 
{
  //Obtain the first header in the heap
  size_t* chunkHeader = heap_mem.start-1;
  //printf("SWEEP: Obtained header of heap: %d\n", chunkHeader);

  while(chunkHeader && chunkHeader < (size_t*)sbrk(0))
  {
    //Obtain the nextChunkHeader
    size_t* nextChunkHeader = next_chunk(chunkHeader);
    //printf("SWEEP: Obtained the next header: %d\n", chunkHeader);

    //If it is marked, no need to free
    if(is_marked(chunkHeader))
    {
     // printf("SWEEP: Chunk: %d is marked, so clearing mark\n", chunkHeader);
      clear_mark(chunkHeader);
    }
    //Not marked and in use...need to free 
    else if(in_use(chunkHeader))
    {
      //Free the payload of the chunkHeader
      //printf("SWEEP: ****SUCCESS**** Freeing the chunkHeader: %d and Payload: %d\n", chunkHeader, (chunkHeader+1));
      free(chunkHeader+1); 
    }
    //Set the currentHeader to be the nextChunkHeader
    chunkHeader = nextChunkHeader;
   // printf("SWEEP: The new chunkHeader: %d\n", chunkHeader);
  }

}



/* IS_POINTER: 
 *
 * Determine if what "looks" like a pointer actually points to a block in the heap
 * Loop through the heap section and check each chunk to see if ptr is contained within that chunk
 * If ptr is contained within a chunk in the heap, return the header of that chunk
 * If ptr is not contained within the heap or is null, return NULL
 *
*/
size_t * is_pointer(size_t* ptr) 
{
 
  //Check to see if the ptr is null
  if(ptr == NULL)
  {
    //printf("IS_POINTER: Returning null because pointer is null\n");
    return NULL;
  }

  //Check to see if hte pointer is within the heap memory otherwise return null
  //Check the address order to make sure that < and >= are correct
  if(ptr< heap_mem.start || ptr>=(size_t*)sbrk(0))
  {
    //printf("IS_POINTER: Returning null because pointer is not contined within the heap\n");
    return NULL;
  }

  //Obtain the chunk header
  size_t* chunkHeader = heap_mem.start - 1;
  //printf("IS_POINTER: Chunk Header of Heap: %d\n ", chunkHeader);
//Loop through the heap memory going chunk by chunk
  while(chunkHeader && chunkHeader<(size_t*)sbrk(0))
  {
    
    //If the ptr is contained within the bounds of the chunk and in use...
                                                                                            //** chunkHeader + chunk_size(chunkHeader) will get you the next chunkHeader
    if(chunkHeader<= ptr && (size_t*)next_chunk(chunkHeader) > ptr && in_use(chunkHeader))
    {
      //Return the header of the chunk containing the pointer
      //printf("IS_POINTER: ****SUCCESSS**** Returning the chunkHeader: %d\n", chunkHeader);
      return chunkHeader;
    }
    //Move on to next chunk header
    chunkHeader = (size_t*)next_chunk(chunkHeader);
   // printf("IS_POINTER: Next Chunk in Heap: %d\n", chunkHeader);
  }
  return NULL;
}



/*  WALK_REGION_AND_MARK: 
*   
*
*Walk through the chunks in the given region (bounded by start-end)
*Obtain the data from the chunks/blocks (of type size_t*) and then see if they are of type pointer
* If type pointer, take the returned block, b, and mark it. 
* (is_pointer returns the header of that chunk soooooo I'm not sure if I can mark each individual block in the chunk???)
* Loop through chunk and recursivley call function on each reoccuring block in the chunk
*/
void walk_region_and_mark(void* start, void* end) 
{
  //Cast the two void pointers
  size_t* startRegion = (size_t*)start;
  size_t* endRegion = (size_t*)end;


  //CurrentRegion will be the iterator looping through the data
  size_t* currentRegion;
  //B will be what is returned from is_pointer
  size_t* b;

  //Loop through the region
  for(currentRegion = startRegion;currentRegion<endRegion;currentRegion++)                //***Not sure if the ++ is correctly incrementing***
  {
    //printf("WALK AND MARK: Current Region: %d\n", currentRegion);

    //Check if the data we are checking is a pointer    
    b = is_pointer((size_t*)*currentRegion);
   // printf("WALK AND MARK: Header Returned by is_pointer: %d\n", b);

    //If b contains a pointer and isn't already marked, then mark it
    if(b!=NULL && !is_marked(b))
    {
        //Mark the header containing the pointer
        mark(b);
        
       // printf("WALK AND MARK: ****SUCCESS**** Marking the Header: %d\n", b);
        
        //Recursivley call walk_region_and_mark on the chunk that corresponds to b
        //Called from chunk payload to end of payload/beginning of next chunk header
        //printf("WALK AND MARK: Recursivley calling with start: %d and end: %d\n", (b+1) , (size_t*)next_chunk(b));
        walk_region_and_mark((b+1),(size_t*)next_chunk(b));
       // printf("***RECURSIVE CALL ENDED***\n");
    }

    
                                                                                    //**CHECK THE NEXT_CHUNK CASE OF RETURNING 0 WHEN GREATER THAN SBRK(0)
  }


}



//------------------------------------**GIVEN FUNCTIONS FUNCTIONS**------------------------------------//




// standard initialization 

void init_gc() 
{
  size_t stack_var;
  init_global_range();
  heap_mem.start=malloc(512);
  //since the heap grows down, the end is found first
  stack_mem.end=(size_t *)&stack_var;
}

void gc() 
{
  size_t stack_var;
  heap_mem.end=sbrk(0);
  //grows down, so start is a lower address
  stack_mem.start=(size_t *)&stack_var;

  // build the index that makes determining valid ptrs easier
  // implementing this smart function for collecting garbage can get bonus;
  // if you can't figure it out, just comment out this function.
  // walk_region_and_mark and sweep are enough for this project.
  //build_heap_index();

  //walk memory regions

  walk_region_and_mark(global_mem.start,global_mem.end);

  walk_region_and_mark(stack_mem.start,stack_mem.end);

  sweep();
}
