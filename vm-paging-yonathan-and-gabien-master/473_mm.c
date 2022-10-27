// Starter code for the page replacement project
#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <signal.h>
#include "473_mm.h"
#include <sys/ucontext.h>

#define READ 0   // only read 
#define WRITE 1 //has both permissions 

//globals///
int rep_policy;
int frames;//frames used
int tot_frames;
int total_vm;
int p_size; //page size
void *vm_start;
///////////////

///structs//
typedef struct fifo// single linked-list
{
   int page;
   int permissions;//0,1,2 based on proj description
   struct fifo *next;
   struct fifo *prev;
   void *addr;  
   int offset;
   int frame; 
}fifo;

typedef struct clock{
  int page;
  int permissions;
  struct clock *next;
  struct clock *prev;
  void *addr;
  int offset;
  int frame;
  int ref;
  int mod;
  int chance;
}clock;

//function prototypes//
static void fifo_handler(int sig, siginfo_t *si, void *context);
static void clock_handler(int sig, siginfo_t *si, void *context);
int concatenate(int page,int phys_addr);
///////////////////////
// fifo functions
fifo *createNode_f(int page, int permissions, void *addr,int offset,int frame)
{
    fifo *ret =(fifo*)malloc(sizeof(fifo));
    ret->next = NULL;
    ret->prev = NULL;
    ret->page = page;
    ret->permissions = permissions;
    ret->addr = addr;
    ret->offset = offset;
    ret->frame = frame;
    return ret;
}

void push_f(fifo *curr,fifo *new)// after
{
    fifo *temp = curr;
    while(temp->next != NULL)
    {
    temp = temp->next;
    }
    temp->next = new;
    temp->next->prev = temp;
    temp->next->next = NULL;//just making sure
    //temp
}

fifo *pop_f(fifo **curr)//pops head 
{
   fifo *retval = NULL;
   fifo *temp = NULL;
   while((*curr) == NULL)
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

int page_here_f(int page,fifo * curr)
{

    fifo *temp = curr;
    while(temp->next != NULL && temp->page != page)
    {
      if(page == temp->page)
      {
      return 1; // good 
      }
      temp = temp->next;
    }
    return 0;// fail
}

// clock functions
clock *createNode_c(int page, int permissions, void *addr,int offset,int frame, int chance, int ref, int mod)
{
    clock *ret =(clock*)malloc(sizeof(clock));
    ret->next = NULL;
    ret->prev = NULL;
    ret->page = page;
    ret->permissions = permissions;
    ret->addr = addr;
    ret->offset = offset;
    ret->chance = chance;
    ret->frame = frame;
    ret->ref = ref;
    ret->mod = mod;

    return ret;
}

void push_c(clock *curr,clock *new)// after
{
    clock *temp = curr;
    while(temp->next != NULL)
    {
    temp = temp->next;
    }
    temp->next = new;
    temp->next->prev = temp;
    temp->next->next = NULL;//just making sure
    //temp
}

//-> remove_by_frame instead of pop
//  loop throught list incrementing chance and checking constraints
clock *pop_c(clock **curr)//pops head 
{
   clock *retval = NULL;
   clock *temp = NULL;
   while((*curr) == NULL)
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

int page_here_c(int page,clock * curr)
{

    clock *temp = curr;
    if(temp->page==page)
    {
    return 1;
    }
    while(temp->next != NULL)
    {
      if(page == temp->page)
      {
      return 1; // good 
      }
      temp = temp->next;
    }
    return 0;// fail
}

clock *popbyVal(clock **curr, int val)// can adjust for different parameter
{
    clock *retval = NULL;
    clock *temp = *curr;
    clock *pre = NULL;//same as below
    clock *nex = NULL;// for swaps
    ///head has val check
    if((*curr)->frame == val)
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
    while(temp->next!=NULL && val != temp->frame) 
    {
    temp = temp->next;
    }
    //tail of list has value
    if(temp->next ==NULL && temp->frame == val)
    {
      retval = temp;
      pre = temp->prev; 
      pre->next = NULL; //point to nothing new tail
      return retval;
    }
    
   //middle linked list node has value
    else if (temp->frame == val)
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
//////////////////////////////////////////////////////////

fifo *fifo_head;
clock *clock_head;  // to reference when jumping back to beging 
clock *current_head; // for eviction
void mm_init(void* vm, int vm_size, int n_frames, int page_size, int policy)
{
        frames = 0;
        rep_policy = policy;
        vm_start = vm;
        total_vm = vm_size;
        tot_frames = n_frames;
        p_size = page_size;
        
        if(rep_policy == 1)
        {
        //fifo 
        fifo_head = (fifo*)malloc(sizeof(fifo));
        fifo_head->prev =NULL;
        fifo_head->next = NULL;
        fifo_head->page = -1;
        fifo_head->permissions = -1;
        fifo_head->frame =0;
        
        struct sigaction sig_act;
        sig_act.sa_flags = SA_SIGINFO;
        sig_act.sa_sigaction = fifo_handler; 
        sigaction(SIGSEGV,&sig_act,NULL);// should jump to function when segfaults
        
        mprotect(vm,vm_size,PROT_NONE);
        }
        else if(rep_policy == 2)
        {
        //third chance 
        clock_head = (clock*)malloc(sizeof(clock));
        current_head = (clock*)malloc(sizeof(clock));
        clock_head->prev = NULL;
        clock_head->next = NULL;
        clock_head->page = -100000;
       
        struct sigaction sig_act;
        sig_act.sa_flags = SA_SIGINFO;
        sig_act.sa_sigaction = clock_handler; 
        sigaction(SIGSEGV,&sig_act,NULL);// should jump to function when segfaults
        
        mprotect(vm,vm_size,PROT_NONE);
        }
}

// handlers///
static void fifo_handler(int sig, siginfo_t *si, void *context)
{
// need to figure out how to get address
ucontext_t *cont = (ucontext_t*)context;  
int reg_err = cont->uc_mcontext.gregs[REG_ERR]; 

int page = (int) (si->si_addr - vm_start) / p_size;// page-1 when entered into logger
int offset = (int) (si->si_addr - vm_start) % p_size;//
//fifo *temp = fifo_head; 
fifo * newNode;
fifo * evicted;
int evict = -1;
int evicted_frame = -1;
int write_back =0;
// reg_err
int bit_zero = 0;
int bit_one = 0;
int fault_type = 0;
// ///////////////

///bit check
bit_zero = reg_err & (0x1);
bit_one = (reg_err >>1 ) &(0x1);
fault_type = bit_zero + bit_one;
//
         ///////  eviction logic  ///////////
          if(frames == 0 && fault_type != 2)
          {
          // first case 
            fifo_head->page = page;
            fifo_head->addr = si->si_addr; //-vm_start;
            fifo_head->offset = offset;
            fifo_head->permissions = fault_type;
            fifo_head->frame = frames;
            frames +=1;
            newNode = fifo_head;
          }
          else if(frames < tot_frames && fault_type !=2)//comment out function if problem
          {
            newNode = createNode_f(page, fault_type, si->si_addr,offset,frames);
            push_f(fifo_head,newNode);
            frames+=1;
          }
          else if(frames == tot_frames && fault_type !=2)
          {
            evicted = pop_f(&fifo_head);
            evict = evicted->page;
            evicted_frame = evicted->frame;
            newNode = createNode_f(page, fault_type, si->si_addr,offset,evicted_frame);
            push_f(fifo_head,newNode);
            mprotect( (evict)*p_size + vm_start,p_size,PROT_NONE);
          if(evicted->permissions != 0)
            {
            write_back =1;
            }
          }
          ///end of eviction logic////
           else if(fault_type ==2)
          {
            newNode = fifo_head;
            int found =0;
            while(newNode->next != NULL && found ==0)
            {
                 if(page == newNode->page)
                 {
                 found = 1;
                 newNode->permissions = 2;
                 }
                 else{
                 newNode = newNode->next;
                  }
                }
            }
          int phys_addr = concatenate(newNode->frame,offset);
          
          //mprotect call///
          if(fault_type ==0)
          {
          mprotect( (newNode->page)*p_size + vm_start,p_size,PROT_READ);
         mm_logger(page, fault_type, evict, write_back,phys_addr);
          }
          else if(fault_type ==1)
          {
          mprotect( (newNode->page)*p_size + vm_start,p_size,PROT_READ | PROT_WRITE);
          mm_logger(page,fault_type, evict,write_back,phys_addr);
          }
          
          else if(fault_type ==2)
          {
          mprotect( (newNode->page)*p_size + vm_start,p_size,PROT_WRITE);//PROT_READ | PROT_WRITE);// if problem could just remove PROT_READ
          mm_logger(page, fault_type, evict, write_back, phys_addr);
          }
          //////// end of mprotect call cases//////////
}
 
static void clock_handler(int sig, siginfo_t *si, void *context)
{
    ucontext_t *cont = (ucontext_t*)context;  
    int reg_err = cont->uc_mcontext.gregs[REG_ERR]; 

    int page = (int) (si->si_addr - vm_start) / p_size;// page-1 when entered into logger
    int offset = (int) (si->si_addr - vm_start) % p_size;

    clock * newNode;
    clock * evicted;
int phys_addr = 0;
    int evict = -1;
    int evicted_frame = -1;
    int write_back =0;
    // reg_err
    int bit_zero = 0;
    int bit_one = 0;
    int fault_type = 0;
    int found = 0;
    // ///////////////

    ///bit check
    bit_zero = reg_err & (0x1);
    bit_one = (reg_err >>1 ) &(0x1);
    fault_type = bit_zero + bit_one;
    //
    if (page_here_c(page,clock_head) == 0)
        {
         ///////  eviction logic  ///////////

            if(frames == 0 && fault_type != 2)
            {
            // first case 
              clock_head->page = page;
              clock_head->addr = si->si_addr; //-vm_start;
              clock_head->offset = offset;
              clock_head->permissions = bit_one;
              clock_head->frame = frames;
              clock_head->chance = 0;
              clock_head->ref = 0;
              clock_head->mod = 0;
              frames +=1;
              newNode = clock_head;
              current_head = clock_head;
               phys_addr = concatenate(0,offset);
               mm_logger(page, fault_type, -1, write_back, phys_addr);
            }
            else if(frames < tot_frames && fault_type !=2)//comment out function if problem
          {
                newNode = createNode_c(page, bit_one, si->si_addr,offset,frames, 0,0,0);
                push_c(clock_head,newNode);
                frames+=1;
               phys_addr = concatenate(frames-1,offset);
              mm_logger(page, fault_type, -1, write_back, phys_addr);
              
          }
          //eviction stuff
          else
          {
            while(found == 0)
            {
             //evicted_frame = current_head->frame;
              if(current_head->ref == 0 && current_head->mod == 0)
              {
              // evict
              found = 1;
              evicted_frame = current_head->frame;
              mprotect((current_head->page)*p_size + vm_start,p_size,PROT_NONE);
              current_head->permissions= READ;
              }
              else if (current_head->ref == 1 && current_head->mod == 0)
              {
              //2nd chance
              current_head->ref=0;
               current_head->chance +=1;
               mprotect( (current_head->page)*p_size + vm_start,p_size,PROT_NONE);
              current_head->permissions= READ;
              }
              else if(current_head->ref == 1 && current_head->mod == 1)
              {
              //third chance
              current_head->ref=0;
               current_head->chance +=1;
               mprotect((current_head->page)*p_size + vm_start,p_size,PROT_NONE);
              current_head->permissions= READ;
              }
              else if ((current_head->ref == 0 && current_head->mod == 1) && (current_head->chance ==1))
              {
              //not eviction canidate yet
               current_head->ref=0;
               current_head->chance +=1;
               mprotect((current_head->page)*p_size + vm_start,p_size,PROT_NONE);
              current_head->permissions= READ;
              }
              else if((current_head->ref == 0 && current_head->mod == 1) && (current_head->chance ==2))
              {
              // evict
              found = 1;
              evicted_frame = current_head->frame;
              mprotect((current_head->page)*p_size + vm_start,p_size,PROT_NONE);
              current_head->permissions= READ;
              }
              
              if(current_head->next == NULL)
                {
                  current_head = clock_head;
                }
              else
                {
                  current_head=current_head->next;
                }
            } 
           newNode = createNode_c(page, bit_one, si->si_addr,offset,evicted_frame, 0,0,0);
          evicted = popbyVal(&clock_head,evicted_frame);
           push_c(clock_head,newNode);
            phys_addr = concatenate(evicted_frame,offset);
           mm_logger(page, fault_type, evicted->page, write_back, phys_addr);
          }
         
           if(bit_one == 0)
           {
            mprotect((newNode->page)*p_size + vm_start,p_size,PROT_READ);
           }
           if(bit_one ==1)
           {
            mprotect((newNode->page)*p_size + vm_start,p_size,PROT_READ|PROT_WRITE);
           }
         }
    else if (page_here_c(page,clock_head) == 1 )
    {
             clock *temp = clock_head; 
             
             while(found ==0)
             {
             if(temp->page == page)
               {
               found = 1;
               newNode = temp;
               }
               if(temp->next == NULL)
               {
               temp= clock_head;
               }
               else
               {
               temp = temp->next;
               }
             }
             phys_addr = concatenate(newNode->frame,offset);
          if( bit_one == 0)
          {
          mprotect((newNode->page)*p_size + vm_start,p_size,PROT_READ);
          mm_logger(page, 3, -1, 0, phys_addr);
          newNode->ref = 1;
          newNode->chance = 0;
          newNode->mod = 0;
          }
          else if(bit_one ==1 && newNode->permissions == WRITE )
          {
          mprotect((newNode->page)*p_size + vm_start,p_size,PROT_WRITE|PROT_READ);
          mm_logger(page, 4, -1, 0, phys_addr);
          newNode->ref = 1;
          newNode->chance = 0;
          newNode->mod = 1;
          }
          else if(bit_one ==1 && newNode->permissions != WRITE)
          {
          mprotect((newNode->page)*p_size + vm_start,p_size,PROT_WRITE);
          mm_logger(page, 2, -1, 0, phys_addr);
          newNode->permissions = WRITE;
          }
    }
}
//////////////
int concatenate(int page,int phys_addr)
{
    int ret = (page%tot_frames)<<12;
    ret = ret | phys_addr;
    return ret;
}