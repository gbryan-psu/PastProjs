//Authours: Gabien Bryan and Yonathan Daniel
//This file has all of our coding effort

// Include files
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define  N_OBJS_PER_SLAB  64
#define FULL 1
#define EMPTY 0
// Functional prototypes
void setup( int malloc_type, int mem_size, void* start_of_memory );
void *my_malloc( int size );
void my_free( void *ptr );
////////////globals///////// 
int type = 0;
int free_mem = 0;
int used_mem = 0;
void *current_addr;
void *start_mem;
int holes =1; 
int total_mem = 1000000;

////////////////////////////
typedef struct objs{
    struct objs *next;
    struct objs *prev;
    int empty;
    void *start;
    void *end;
    void *retptr;
}objs;

typedef struct slab{    //slab descriptor table
    int size;
    int type;
    int empty;
    int used;
    //objs *obj_header;
    int *obj_empty[N_OBJS_PER_SLAB];
    void *objects[N_OBJS_PER_SLAB];
    void *start;
    void *end;
    void *retptr;
}slab;

typedef struct s_node{
    struct s_node *next;
    struct s_node *prev;
    slab header;
    int block_size;
} s_node;

typedef struct data{ 
    int size;
    int type;
    int empty; //is this chunk empty of not
    void *start;
    void *end; 
    void *retptr;
}data; 

typedef struct node{
    struct node *next;
    struct node *prev;
    //int size; //actual data size in block
    data header;
    int block_size;// some power of 2
} node;


/////notes////////
//gotta adjust pop by val for specific struct param
//
//linked list functs/////
node *createNode(data val)
{
    node *ret =(node*)malloc(sizeof(node));
    ret->next = NULL;
    ret->prev = NULL;
    ret->header = val;
    return ret;
}
s_node *s_createNode(slab val)
{
    s_node *ret =(s_node*)malloc(sizeof(s_node));
    ret->next = NULL;
    ret->prev = NULL;
    ret->header = val;
    return ret;
}
void push(node *curr,node *new)// after
{
    node *temp = curr;
    if (temp->next != NULL)
    {
        node *temp2 = temp->next;
        temp->next = new;
        temp->next->prev = temp;
        temp->next->next = temp2;
        temp2->prev = new;

    }
    else{
        temp->next = new;
        temp->next->prev = temp;
        temp->next->next = NULL;//just making sure
    }
    //temp
}
void s_push(s_node *curr,s_node *new)// after
{
    s_node *temp = curr;
    if (temp->next != NULL)
    {
        s_node *temp2 = temp->next;
        temp->next = new;
        temp->next->prev = temp;
        temp->next->next = temp2;
        temp2->prev = new;

    }
    else{
        temp->next = new;
        temp->next->prev = temp;
        temp->next->next = NULL;//just making sure
    }
    //temp
}
void o_push(objs *curr,objs *new)// after
{
    objs *temp = curr;
    if (temp->next != NULL)
    {
        objs *temp2 = temp->next;
        temp->next = new;
        temp->next->prev = temp;
        temp->next->next = temp2;
        temp2->prev = new;

    }
    else{
        temp->next = new;
        temp->next->prev = temp;
        temp->next->next = NULL;//just making sure
    }
    //temp
}
node *pop(node **curr)//pops head 
{
   node *retval = NULL;
   node *temp = NULL;
        if((*curr)==NULL)
            {
            return retval; 
            }
   temp = (*curr)->next;
   retval = (*curr);
   //free(*curr); bad practice 
   // probably where our og linked list went wrong
    *curr = temp;
    (*curr)->prev = NULL;// remove prev val
    return retval;
}
node *popbyVal(node **curr, void *val)// can adjust for different parameter
{
    node *retval = NULL;
    node *temp = *curr;
    node *pre = NULL;//same as below
    node *nex = NULL;// for swaps
    ///head has val check
    if((*curr)->header.retptr == val)
    {
      temp = (*curr)->next;
   retval = (*curr);
   //free(*curr); bad practice 
   // probably where our og linked list went wrong
    *curr = temp;
    (*curr)->prev = NULL;// remove prev val
    return retval;
    }
    //
    while(temp->next!=NULL && val != temp->header.retptr) 
    {
    temp = temp->next;
    }
    //tail of list has value
    if(temp->next ==NULL && temp->header.retptr == val)
    {
       retval = temp;
       pre = temp->prev; 
      pre->next = NULL; //point to nothing new tail
      return retval;
    }
    
   //middle linked list node has value
    else if (temp->header.retptr == val)
    {
       retval = temp;
       pre = temp->prev; 
       nex = temp->next;
       //could just free tbh maybe?
        pre->next = nex; //reconnect list
        nex->prev = pre;
    return retval;
    }
    
    //assuming value is always in list tbh
}
s_node *s_popbyVal(s_node **curr, void *val)// can adjust for different parameter
{
    s_node *retval = NULL;
    s_node *temp = *curr;
    s_node *pre = NULL;//same as below
    s_node *nex = NULL;// for swaps
    ///head has val check
    if((*curr)->header.retptr == val)
    {
      temp = (*curr)->next;
   retval = (*curr);
   //free(*curr); bad practice 
   // probably where our og linked list went wrong
    *curr = temp;
    (*curr)->prev = NULL;// remove prev val
    return retval;
    }
    //
    while(temp->next!=NULL && val != temp->header.retptr) 
    {
    temp = temp->next;
    }
    //tail of list has value
    if(temp->next ==NULL && temp->header.retptr == val)
    {
       retval = temp;
       pre = temp->prev; 
      pre->next = NULL; //point to nothing new tail
      return retval;
    }
    
   //middle linked list node has value
    else if (temp->header.retptr == val)
    {
       retval = temp;
       pre = temp->prev; 
       nex = temp->next;
       //could just free tbh maybe?
        pre->next = nex; //reconnect list
        nex->prev = pre;
    return retval;
    }
    
    //assuming value is always in list tbh
}
//
int power(int base,int exp){
     int val = 1;
     for(int i = 1;i<=exp;i++){
        val = val*base;
     }
     return val;
}
int get_power(int size){
    int exp = -1;
    for (int i = 0; i < 21; i ++){
        if (size <= power(2, i) && size > power(2, i-1)){
            exp = i;
            break;
        }
    }

    if (exp < 10){
        exp = 10;
    }
    return exp;
}

//////////////////
node *alloc;
s_node *s_alloc;
//node *free;

////////////////////////////////////////////////////////////////////////////
//
// Function     : setup
// Description  : initialize the memory allocation system
//
// Inputs       : malloc_type - the type of memory allocation method to be used [0..3] where
//                (0) Buddy System
//                (1) Slab Allocation

void setup( int malloc_type, int mem_size, void* start_of_memory ){
    if (malloc_type == 0){
            type = malloc_type;
            free_mem = 1000000;//mem_size; //1 MB
            current_addr = start_of_memory;// global mem pointer
            alloc = (node*)malloc(sizeof(node));
            alloc->next = NULL;
            alloc->prev = NULL;
            int exp = get_power(free_mem);
            alloc->block_size = power(2, exp);
            alloc->header.size = free_mem;
            alloc->header.empty = EMPTY;
            alloc->header.start = start_of_memory;
            alloc->header.end = start_of_memory + 999999; //for now
    }
    else if (malloc_type == 1){
        type = malloc_type;
        free_mem = 1000000;

        s_alloc = (s_node*)malloc(sizeof(s_node));
        s_alloc->next = NULL;
        s_alloc->prev = NULL;
        int exp = get_power(free_mem);
        s_alloc->block_size = power(2, exp);
        s_alloc->header.empty = EMPTY;
        s_alloc->header.start = start_of_memory;
        s_alloc->header.end = start_of_memory + s_alloc->block_size;
        s_alloc->header.used = 0;
        /*
        s_alloc->header.obj_header = (objs*)malloc(sizeof(objs));
        s_alloc->header.obj_header->empty = EMPTY;
        s_alloc->header.obj_header->next = NULL;
        s_alloc->header.obj_header->prev = NULL;
        */
    }
}

////////////////////////////////////////////////////////////////////////////
//
// Function     : my_malloc
// Description  : allocates memory segment using specified allocation algorithm
//
// Inputs       : size - size in bytes of the memory to be allocated
// Outputs      : -1 - if request cannot be made with the maximum mem_size requirement

void *my_malloc( int size ){

    if (type == 0){
        int new_size = size + 4;
        int block_size = 0;
        int temp_size = 0;
        int hole_found = 0;
        int exp = 0;//power and pow can't be used in our var's
        int pointer = 0;
        node *new_block;
        node *temp;
        //for while loop///
        node *newNode;
        data storage;// basically header values
        //
        int *ret;
        exp = get_power(new_size); 
        block_size = power(2, exp);
        temp = alloc;
        if (used_mem + block_size > 1048576){
            return (void *)(-1);
        }
        //("size of block request: %d\n",block_size);  
        if (holes>1)
        {
            //("current address: %p \n",current_addr);
            while(temp != NULL && hole_found == 0)//check if another node 
            {
              //("In while loop\n");
              //for debugging///
              if(temp->header.empty ==EMPTY)
              {
                //("block is definetly empty\n");
                //("block size: %d\n",temp->block_size);
              }
              if(temp->block_size==block_size)
              {
                //("block size works\n");
              }
              if(temp->block_size>block_size){
                  //("block size larger: creat buddy\n");
              }
              ///
               if((temp->header.empty ==EMPTY) &&(temp->block_size==block_size))//checks if block can fit 
               {
                 temp->block_size = block_size;//block size
                 temp->header.size = new_size;
                 temp->header.empty = FULL;
                 free_mem-=block_size;
                 used_mem+=block_size;
                 holes--;
                 temp->header.end = temp->header.start + (block_size-1);
                 ret = temp->header.start + 4;//temp->header.start;
                 temp->header.retptr = ret;
                hole_found = 1; 
                return ret; 
               }
               else if((temp->header.empty ==EMPTY) && (temp->block_size>block_size))// if we have to split a block
               {
                   node *temp2;
                   temp2 = temp;
                   while (temp2->next != NULL){
                       if (temp2->block_size < temp->block_size && temp2->block_size >= block_size && temp2->header.empty == EMPTY){
                           temp =temp2;
                       }
                       temp2 = temp2->next;
                   }
                   temp_size = temp->block_size;
                   temp->block_size = block_size;
                   temp->header.size = new_size;
                   temp->header.empty = FULL;
                   free_mem-=block_size;
                   used_mem+=block_size;
                   holes--;

                   temp->header.end = temp->header.start + (block_size-1);
                   ret = temp->header.start + 4;
                   temp->header.retptr = ret;
                   temp_size = temp_size/2;
                    while (block_size <= temp_size){
                        storage.size = block_size;
                        storage.empty = EMPTY;
                        storage.start = temp->header.end + 1;
                        storage.end = storage.start + (block_size - 1);

                        newNode = createNode(storage);
                        newNode->block_size = block_size;
                        push(temp, newNode);
                        holes++;

                        temp = temp->next;
                        block_size = block_size * 2;

                    }

                   hole_found = 1;
                   return ret;
               }
               temp = temp->next; 
            }
            //return (void *)(-1);
        }
        else// first malloc or full 1MB available
        {
         temp_size = alloc->block_size;
         alloc->block_size = block_size;//block size
         alloc->header.size = new_size;
         alloc->header.empty = FULL;
         alloc->header.end = alloc->header.start + (block_size -1);
         free_mem-=block_size;
         used_mem+=block_size;

        temp_size = temp_size/2;
        temp = alloc;

        while (block_size <= temp_size){
            storage.size = block_size;
            storage.empty = EMPTY;
            storage.start = temp->header.end + 1;
            storage.end = storage.start + (block_size - 1);

            newNode = createNode(storage);
            newNode->block_size = block_size;
            push(temp, newNode);
            holes++;

            temp = temp->next;
            block_size = block_size * 2;

            }

         ret = alloc->header.start + 4;
         alloc->header.retptr = ret;
         return ret;
        } 
    }

    
    if (type == 1){
        int new_size = 0;
        int slab_size = 0;
        int temp_size = 0;
        int obj_size = 0;
        int hole_found = 0;
        int exp = 0;
        s_node *temp;
        slab storage;
        s_node *newNode;
        objs *newObj;
        objs *tempObj;

        int *ret;
        obj_size = size + 4;
        

        if (holes>1){
            if (used_mem + obj_size > 1048576){
                return (void *)(-1);
            }
            temp = s_alloc;
            while (temp != NULL && hole_found == 0){
                if (temp->header.type == size){
                    //("same type");
                }
                if (temp->header.used < 64){
                    //("less than");
                }
                if (temp->header.type == size && temp->header.used < 64){
                    //("used: %d\n", temp->header.used);
                    //("made it here\n");
                    int index = temp->header.used;
                    for (int i = 0; i < 64; i++){
                        if (temp->header.obj_empty[i] == (int *)-1){
                            index = i;
                            break;
                        }
                    }
                    //int index = temp->header.used;
                    //if (temp->header.obj_empty[index]== (int *)-1){
                        if (index == 0){
                            temp->header.objects[index]= temp->header.start + 8;
                            ret = temp->header.objects[index];
                            temp->header.obj_empty[index] = 0;
                            return ret;
                        }
                        else {
                            temp->header.objects[index] = temp->header.objects[index-1] + temp->header.type + 4;
                            ret = temp->header.objects[index];
                            temp->header.used++;
                            temp->header.obj_empty[index] = 0;
                            return ret;
                        }
                }
                
                
                if (temp->next == NULL){
                    break;
                }
                temp = temp->next;
            }
            temp = s_alloc;
            new_size = obj_size * 64;
            exp = get_power(new_size);
            slab_size = power(2,exp);
            while (temp != NULL && hole_found == 0){
                //("(1) type: %d  size: %d  used: %d\n", temp->header.type, size, temp->header.used);
                if (temp->header.type == size && temp->header.used < 64){
                    int index = temp->header.used;
                    for (int i = 0; i < 64; i++){
                        if (temp->header.obj_empty[i] == (int *) -1){
                            index = i;
                            break;
                        }
                    }
                    //("Type and size eq, used < 64\n index: %p", temp->header.objects[index]);
                    //if (temp->header.obj_empty[index]== -1){
                        if (index == 0){
                            temp->header.objects[index]= temp->header.start + 8;
                            ret = temp->header.objects[index];
                            temp->header.obj_empty[index] = 0;
                            temp->header.used++;
                    
                            return ret;
                        }
                        else {
                            temp->header.objects[index] = temp->header.objects[index-1] + temp->header.type + 4;
                            ret = temp->header.objects[index];
                            temp->header.used++;
                            temp->header.obj_empty[index] = 0;
                     
                            return ret;
                        }
                    //}
                }
                
                if (temp->block_size == slab_size && temp->header.empty == EMPTY){
                    //("1: bs-%d ss-%d\n", temp->block_size, slab_size);
                    temp->header.empty = FULL;
                    temp->header.type = size;
                    temp->header.used = 1;
                    temp->header.retptr = temp->header.start + 4;
                    /*
                    temp->header.obj_header->empty = FULL;
                    temp->header.obj_header->start = temp->header.start+4;
                    temp->header.obj_header->end = temp->header.obj_header->start + (obj_size - 1);
                    temp->header.obj_header->retptr = temp->header.obj_header->start + 4;
                    ret = temp->header.obj_header->retptr;
                    */
                   temp->header.objects[0] = (void *)malloc(sizeof(void));
                   temp->header.obj_empty[0] = (int *)malloc(64*sizeof(void));
                   temp->header.objects[0] = temp->header.start + 8;
                   ret = temp->header.objects[0];
                   temp->header.obj_empty[0] = 0;
                    free_mem -= obj_size;
                    used_mem += obj_size;

                    
                    //tempObj = temp->header.obj_header;
                    for (int i = 1; i < 64; i++){
                        if(i < 63){
                            //tempObj->next = newObj;
                        }
                        /*
                        newObj->empty = EMPTY;
                        newObj->next = NULL;
                        newObj->prev = tempObj;
                        newObj->start = tempObj->end + 1;
                        newObj->end = newObj->start + (size-1);
                        newObj->retptr = newObj->start+4;
                        o_push(tempObj,newObj);
                        */
                       temp->header.objects[i] = (void *)malloc(sizeof(void));
                       temp->header.obj_empty[i] = (int *)malloc(64*sizeof(void));
                        temp->header.obj_empty[i] = (int *) -1;
                    }
    
                    return ret;
                }
                else if (temp->block_size > slab_size && temp->header.empty == EMPTY){
                //("2\n");
                   temp_size = temp->block_size;
                   temp->block_size = slab_size;
                   temp->header.size = slab_size;
                   temp->header.type = size;
                   temp->header.used = 1;
                   temp->header.end = temp->header.start + (slab_size-1);
                   temp->header.retptr = temp->header.start + 4;
                   temp->header.empty = FULL;
                   temp->header.objects[0] = (void *)malloc(sizeof(void));
                       temp->header.obj_empty[0] = (int *) malloc(sizeof(int));
                   temp->header.obj_empty[0] = (int *) 0;
                    /*
                   temp->header.obj_header->empty = FULL;
                    temp->header.obj_header->start = temp->header.start+4;
                    temp->header.obj_header->end = temp->header.obj_header->start + (obj_size - 1);
                    temp->header.obj_header->retptr = temp->header.obj_header->start + 4;
                    ret = temp->header.obj_header->retptr;
                    */
                   temp->header.objects[0] = temp->header.start + 8;
                   ret = temp->header.objects[0];
                   free_mem-=slab_size;
                   used_mem+=slab_size;
                   holes--;

                   temp_size = temp_size/2;
                    while (slab_size <= temp_size){
                        storage.size = slab_size;
                        storage.empty = EMPTY;
                        storage.start = temp->header.end + 1;
                        storage.retptr = storage.start + 4;
                        storage.end = storage.start + (slab_size - 1);
                        storage.used = 1;

                        newNode = s_createNode(storage);
                        newNode->block_size = slab_size;
                        s_push(temp, newNode);
                        holes++;

                        temp = temp->next;
                        slab_size = slab_size * 2;
                    }
                    //tempObj = temp->header.obj_header;
                    for (int i = 1; i < 64; i++){
                        if(i < 63){
                            //tempObj->next = newObj;
                        }
                        /*
                        newObj->empty = EMPTY;
                        newObj->next = NULL;
                        newObj->prev = tempObj;
                        newObj->start = tempObj->end + 1;
                        newObj->end = newObj->start + (size-1);
                        newObj->retptr = newObj->start+4;
                        o_push(tempObj, newObj);
                        */
                       temp->header.objects[i] = (void *)malloc(sizeof(void));
                       temp->header.obj_empty[i] = (int *) malloc(sizeof(int));
                       temp->header.obj_empty[i] = (int *) -1;
                    }
                   hole_found = 1;
                   //("(2)type: %d  size: %d  used: %d\n", temp->header.type, size, temp->header.used);
                   return ret;

                }
                
                temp = temp->next;
            }
     
            //return (void *)(-1);
        }
        else{ //first malloc or 1MB empty
            
            new_size = obj_size * 64;
            exp = get_power(new_size); 
            slab_size = power(2, exp);
            //("3: ss- %d ns- %d \n", slab_size, new_size);
            temp = s_alloc;
            if (used_mem + slab_size > 1048576){
                return (void *)(-1);
            }
            
            temp_size = s_alloc->block_size;
            s_alloc->block_size = slab_size;
            s_alloc->header.type = size;
            s_alloc->header.used = 1;
            //("used: %d\n", temp->header.used);
            //("used: %d\n", s_alloc->header.used);
            s_alloc->header.end = s_alloc->header.start + (slab_size - 1);
            s_alloc->header.retptr = s_alloc->header.start + 4;
            s_alloc->header.size = slab_size;
            s_alloc->header.empty = FULL;
            /*
            s_alloc->header.obj_header->empty = FULL;
            s_alloc->header.obj_header->start = s_alloc->header.start + 4;
            s_alloc->header.obj_header->end = s_alloc->header.obj_header->start + (obj_size-1);
            s_alloc->header.obj_header->retptr = s_alloc->header.obj_header->start + 4;
            ret = s_alloc->header.obj_header->retptr;
            */

           temp->header.objects[0] = (void *)malloc(sizeof(void));
            temp->header.obj_empty[0] = (int *)malloc(sizeof(int));
            s_alloc->header.objects[0] = s_alloc->header.start+8;
            ret = s_alloc->header.objects[0];
            s_alloc->header.obj_empty[0] = (int *) 1;

            free_mem -= obj_size;
            used_mem += obj_size;

            temp_size = temp_size/2;
            temp = s_alloc;
            //while (slab_size <= temp_size){
                storage.size = slab_size;
                storage.empty = EMPTY;
                storage.start = temp->header.end + 1;
                storage.retptr = storage.start + 4;
                storage.end = storage.start + (slab_size - 1);
                storage.used = 1;

                newNode = s_createNode(storage);
                newNode->block_size = slab_size;
                s_push(temp, newNode);
                holes++;

                temp = temp->next;
                //("used: %d\n", temp->header.used);
                slab_size = slab_size * 2;
            //}
            //tempObj = s_alloc->header.obj_header;
            for (int i = 1; i < 64; i++){
                
                if(i < 63){
                    //tempObj->next = newObj;
                }
                /*
                newObj->empty = EMPTY;
                newObj->next = NULL;
                newObj->prev = NULL;
                newObj->start = tempObj->end + 1;
                newObj->end = newObj->start + (size-1);
                newObj->retptr = newObj->start+4;
                o_push(tempObj,newObj);
                */
               temp->header.objects[i] = (void *)malloc(sizeof(void));
                       temp->header.obj_empty[i] = (int *)malloc(sizeof(int));

               s_alloc->header.obj_empty[i] = (int *) -1;
  
            }
            //("used: %d\n", temp->header.used);
            //("used: %d\n", s_alloc->header.used);
            ("done");
            return(ret);
        }
        //return (void *)(-1);
        //("4\n");

    }

}

////////////////////////////////////////////////////////////////////////////
//
// Function     : my_free
// Description  : deallocated the memory segment being passed by the pointer
//
// Inputs       : ptr - pointer to the memory segment to be free'd
// Outputs      :

void my_free( void *ptr )
{
    int size = 0;
    int next_size = 0;
    int prev_size = 0;
    int new_size = 0;
    int buddy = 1;
    int obj_size = 0;
    int empty_slab = 0;



    if (type == 0){
        //find node in linked list with start pointer = ptr - 4
        //set prev nodes size to prev_size
        //set next nodes size to next_size
        node *temp;
        temp = alloc;
        while (temp->next != NULL && temp->header.retptr != ptr){
            temp = temp->next;
        }
        while (buddy == 1){
            int loop_finish = 0;
            if (temp->header.retptr == ptr){
                //("Block Found\n");
            }
            //("wtf\n");
            size = temp->block_size;
            free_mem += size;
            used_mem -= size;
            //("size: %d\n", temp->block_size);
            if (temp->prev != NULL){
                prev_size = temp->prev->block_size;
                //("prev_size: %d\n", temp->prev->block_size);
            }
            if (temp->next != NULL){
                next_size = temp->next->block_size;
                //("next_size: %d\n", temp->next->block_size);
            }
            if (prev_size == size && temp->prev->header.empty == EMPTY){
                //("1\n");
                new_size = prev_size + size;
                temp->prev->block_size = new_size;
                temp->prev->header.end = temp->header.end;
                ptr = temp->header.retptr;
                temp = temp->prev;
                loop_finish = 1;
                popbyVal(&alloc, ptr);
            }
            else if (next_size == size && temp->next->header.empty == EMPTY && loop_finish == 0){
                //("2\n");
                new_size = next_size+size;
                temp->next->block_size = new_size;
                temp->next->header.start = temp->header.start;
                ptr = temp->header.retptr;
                temp = temp->next;
                loop_finish = 1;
                popbyVal(&alloc, ptr);
            }
            else if ((next_size != size && prev_size != size && loop_finish == 0) || (temp->next->header.empty == FULL && loop_finish == 0) || (temp->prev->header.empty == FULL && loop_finish == 0)){
                //("Made it here\n");
                temp->header.empty = EMPTY;
                buddy = 0;
            }
        }
        //("finished\n");
    }

    if (type == 1){
        s_node *s_temp;
        int obj_index;
        int slab_size;
        s_temp = s_alloc;
        while (s_temp->next != NULL){
            for (int i = 0; i <64; i++){
                if (s_temp->header.objects[i] == ptr){
                    obj_index = i;
                    break;
                }
            }
            s_temp = s_temp->next;
        }
        size = s_temp->block_size;
        int loop_finish = 0;
       
       //while (buddy == 1){
            if (s_temp->header.objects[obj_index] == ptr){
                ("found object correctly");
            }
            //("made it here\n");
            s_temp->header.obj_empty[obj_index] = (int *)(-1);
            obj_size = s_temp->header.type + 4;
            free_mem += obj_size;
            used_mem -= obj_size;

            for(int i = 0; i < 64; i++){
                if (s_temp->header.obj_empty[i] == (int *)(-1)){
                    empty_slab ++;
                }
            } 
            if (empty_slab == 64){ //slab empty need to set to empty and possibly merge
                if (s_temp->prev != NULL){
                    prev_size = s_temp->prev->block_size;
                    //("prev_size: %d\n", temp->prev->block_size);
                }
                if (s_temp->next != NULL){
                    next_size = s_temp->next->block_size;
                    //("next_size: %d\n", temp->next->block_size);
                }
                if (prev_size == size && s_temp->prev->header.empty == EMPTY){
                    //("1\n");
                    new_size = prev_size + size;
                    s_temp->prev->block_size = new_size;
                    s_temp->prev->header.end = s_temp->header.end;
                    ptr = s_temp->header.retptr;
                    s_temp = s_temp->prev;
                    loop_finish = 1;
                    s_popbyVal(&s_alloc, ptr);
                }
                else if (next_size == size && s_temp->next->header.empty == EMPTY && loop_finish == 0){
                    //("2\n");
                    new_size = next_size+size;
                    s_temp->next->block_size = new_size;
                    s_temp->next->header.start = s_temp->header.start;
                    ptr = s_temp->header.retptr;
                    s_temp = s_temp->next;
                    loop_finish = 1;
                    s_popbyVal(&s_alloc, ptr);
                }
                else if ((next_size != slab_size && prev_size != slab_size && loop_finish == 0) || (s_temp->next->header.empty == FULL && loop_finish == 0) || (s_temp->prev->header.empty == FULL && loop_finish == 0)){
                    //("Made it here\n");
                    s_temp->header.empty = EMPTY;
                    buddy = 0;
                }
            }
            else
            {
 
                buddy == 0;
            }
        //}*/
    }
}
