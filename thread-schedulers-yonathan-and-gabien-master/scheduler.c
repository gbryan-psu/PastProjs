/**
*
* File             : scheduler.c
* Description      : This is a stub to implement all your scheduling schemes
*
* Author(s)        : @Yonathan_Daniel and Gabien Bryan
* Last Modified    : @10/12/2020
*/
 
// Include Files
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <time.h>
 
#include <math.h>
#include <pthread.h>
#include <semaphore.h>// heard we could add it
 
void init_scheduler( int sched_type );
int schedule_me( float currentTime, int tid, int remainingTime, int tprio );
int P( float currentTime, int tid, int sem_id);
int V( float currentTime, int tid, int sem_id);
 
#define FCFS    0
#define SRTF    1
#define PBS     2
#define MLFQ    3
#define SEM_ID_MAX 50
 
// globals
int typesched = 0;       //global variable to decided what scheduling will operate
int g_currentTime = 0;
//float g_currentTime = 0; //gloabl variable for current time
int recieved_tids[SEM_ID_MAX] = {0}; //initialize to zero
int Tcount = 0; // number of threads
int Bcount=0;// block queue count
int Rcount = 0;
int lock = 0;// 1 for locked 0for unlocked
int MLFQ_count = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
//linked list
typedef struct Node{
int tid;
float current_Time;// stores time it entered programing
int remainingTime;
int tprio;// -1 unless in pbs
pthread_cond_t cond;
}  Node;
 
//
Node running;
Node Blocked[SEM_ID_MAX]= {0};
Node Ready[SEM_ID_MAX]={0};
 
// may have to add count parameters for case with multiple ready/block queues
void push_blocked(Node *ptr,Node added)
{
   if(Bcount==0)
   {
       ptr[0] = added;
       Bcount+=1;
       //printf("first case push\n");
       //printf("tid(%d) in blocked\n",added.tid);
   }
  
   else if(Bcount >0)
   {
       //printf("2nd case push\n");
       for(int i = 0; i <Bcount;i++)
       {
           if(ptr[i].tid==-1)
           {
       //      printf(" Successful push\n");
               ptr[i] = added;
               Bcount+=1;
       //      printf("tid(%d) in blocked \n",added.tid);
       //      printf("Bcount:(%d)\n",Bcount);
       //      printf("pushed ptr[%d] ,tid(%d),arrival(%f),remaing(%d),tpri(%d)\n",i,ptr[i].tid,ptr[i].current_Time,ptr[i].remainingTime,ptr[i].tprio);
               break;
           }
       //  printf("not pushed ptr[%d] ,tid(%d),arrival(%f),remaing(%d),tpri(%d)\n",i,ptr[i].tid,ptr[i].current_Time,ptr[i].remainingTime,ptr[i].tprio);
       }  
   }
}
Node pop_blocked(Node *ptr)
{
   Node ret;
   for(int i = 0; i <Bcount;i++)
       {
           if( ptr[i].tid!=-1)
           {
       //      printf("succesful pop\n");
       //      printf("ptr[%d] ,tid(%d),arrival(%f),remaing(%d),tpri(%d)\n",i,ptr[i].tid,ptr[i].current_Time,ptr[i].remainingTime,ptr[i].tprio);
               ret = ptr[i];
               ptr[i].current_Time = -1;
               ptr[i].remainingTime = -1;
               ptr[i].tid = -1;
               ptr[i].tprio = -1;
               Bcount-=1;
       //      printf("Bcount:(%d)\n",Bcount);
               return ret;
           }
          
       }           
}
void push_ready(Node *ptr,Node added)
{
   if(Rcount==0)
   {
       ptr[0] = added;
       Rcount+=1;
//      printf("first case push\n");
//      printf("tid(%d) in ready\n",added.tid);
   }
  
   else if(Rcount >0)
   {
//      printf("2nd case push\n");
       for(int i = 0; i <Bcount;i++)
       {
           if(ptr[i].tid==-1)
           {
//              printf(" Successful push\n");
               ptr[i] = added;
               Rcount+=1;
//              printf("tid(%d) in ready \n",added.tid);
//              printf("Rcount:(%d)\n",Rcount);
//              printf("pushed ptr[%d] ,tid(%d),arrival(%f),remaing(%d),tpri(%d)\n",i,ptr[i].tid,ptr[i].current_Time,ptr[i].remainingTime,ptr[i].tprio);
               break;
           }
//          printf("not pushed to ready ptr[%d] ,tid(%d),arrival(%f),remaing(%d),tpri(%d)\n",i,ptr[i].tid,ptr[i].current_Time,ptr[i].remainingTime,ptr[i].tprio);
       }  
   }
}
Node pop_ready(Node *ptr)
{
   Node ret;
 
   for(int i = 0; i <SEM_ID_MAX;i++)
       {
           if( ptr[i].tid!=-1)
           {
//              printf("succesful pop\n");
//              printf("ptr[%d] ,tid(%d),arrival(%f),remaing(%d),tpri(%d)\n",i,ptr[i].tid,ptr[i].current_Time,ptr[i].remainingTime,ptr[i].tprio);
               ret = ptr[i];
               ptr[i].current_Time = -1;
               ptr[i].remainingTime = -1;
               ptr[i].tid = -1;
               ptr[i].tprio = -1;
               Rcount-=1;
//              printf("Rcount:(%d)\n",Bcount);
               return ret;
           }      
       }  
}
Node pop_block_id(Node *ptr, int tid){
  Node ret;
   for(int i = 0; i <Bcount;i++)
      {
          if(ptr[i].tid == tid)
          {
            
              ret = ptr[i];
              ptr[i].current_Time = -1;
              ptr[i].remainingTime = -1;
              ptr[i].tid = -1;
              ptr[i].tprio = -1;
              Bcount-=1;
              return ret;
          }
      }
}
int tid_in_blocked(Node *ptr,int tid)
{
   for(int i = 0; i <SEM_ID_MAX;i++)
       {
           if(ptr[i].tid==tid)
           {
//          printf("tid(%d) in blocked spot:(%d)\n",tid,i);
               return 1;
           }
       }
//          printf("tid(%d) NOT in blocked\n",tid);
   return 0;
}
int tid_in_ready(Node *ptr,int tid)
{
   for(int i = 0; i <SEM_ID_MAX;i++)
       {
           if(ptr[i].tid==tid)
           {
//          printf("tid(%d) in ready spot:(%d)\n",tid,i);
               return 1;
           }
       }
//          printf("tid(%d) NOT in ready\n",tid);
   return 0;
}
 
//sema_t sem[SEM_ID_MAX];
//inr sem_count = 0;// move to top later after meeting a ta
int find_tid( int tid ){
 
     for(int i=0; i<= Tcount; i++){
      if( tid == recieved_tids[i]){
         // printf("found tid:(%d)\n",tid);
          return 1;
      }
  }
 //  printf("not found tid:(%d)\n",tid);
  recieved_tids[Tcount] = tid;
  Tcount++;
  return 0;
}
 
void sortbyRemainingTime(Node *list)
{  
   //bubble sort
   Node temp;
   int swap = 1;
   while (swap == 1)
   {
     swap = 0;// makes abovestatement false  unless a swap occurs in the for loop
     for(int i = 0;i<SEM_ID_MAX- 1; i++)
     {
       if(list[i].remainingTime>list[i+1].remainingTime)//conditional checks if two values are out of place
       {
         temp = list[i+1];
         list[i+1]= list[i];
         list[i] = temp;
         swap = 1; // swap is true so while loop keeps going
       }
     }
   }
}
void sortbyPrio(Node *list)
{  
   //bubble sort
   Node temp;
   int swap = 1;
   while (swap == 1)
   {
     swap = 0;// makes abovestatement false  unless a swap occurs in the for loop
     for(int i = 0;i<SEM_ID_MAX - 1; i++)
     {
       if(list[i].tprio<list[i+1].tprio)//conditional checks if two values are out of place
       {
         temp = list[i+1];
         list[i+1]= list[i];
         list[i] = temp;
         swap = 1; // swap is true so while loop keeps going
       }
     }
   }
 
}
void lockFCFS(int tid)
{
    pthread_mutex_lock(&mutex);
   lock = 1;
   while(lock ==1){
       pthread_cond_wait(&cond,&mutex);
       if(running.tid !=tid)//==tid)
       {
//          running.remainingTime--;
       //  printf("break from lock function\n");
           //  break;
       }
   }
 
   pthread_mutex_unlock(&mutex);
}
 
void unlockFCFS()
{
   pthread_mutex_lock(&mutex);
   lock = 0;
   pthread_cond_signal(&cond);//pthread_cond_broadcast(&cond);
   pthread_mutex_unlock(&mutex);
}
 
void lockSRTF(int tid)
{
    pthread_mutex_lock(&mutex);
   lock = 1;
   while(lock ==1){
       pthread_cond_wait(&cond,&mutex);
       if(running.tid !=tid)//==tid)
       {
//          running.remainingTime--;
       //printf("break from lock function\n");
               //break;
       }
   }
 
   pthread_mutex_unlock(&mutex);
}
 
void unlockSRTF()
{
   pthread_mutex_lock(&mutex);
   lock = 0;
   pthread_cond_signal(&cond);//pthread_cond_broadcast(&cond);
   pthread_mutex_unlock(&mutex);
}
void lockPBS(int tid)
{
    pthread_mutex_lock(&mutex);
   lock = 1;
   while(lock ==1){
       pthread_cond_wait(&cond,&mutex);
       if(running.tid !=tid)//==tid)
       {
//          running.remainingTime--;
       //printf("break from lock function\n");
               //break;
       }
   }
 
   pthread_mutex_unlock(&mutex);
}
 
void unlockPBS()
{
   pthread_mutex_lock(&mutex);
   lock = 0;
   pthread_cond_signal(&cond);//pthread_cond_broadcast(&cond);
   pthread_mutex_unlock(&mutex);
}
void lockMLFQ(int tid)
{
    pthread_mutex_lock(&mutex);
   lock = 1;
   while(lock ==1){
       pthread_cond_wait(&cond,&mutex);
       if(running.tid !=tid)//==tid)
       {
//          running.remainingTime--;
       //printf("break from lock function\n");
               //break;
       }
   }
 
   pthread_mutex_unlock(&mutex);
}
 
void unlockMLFQ()
{
   pthread_mutex_lock(&mutex);
   lock = 0;
   pthread_cond_signal(&cond);//pthread_cond_broadcast(&cond);
   pthread_mutex_unlock(&mutex);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void init_scheduler( int sched_type ) {
   typesched = sched_type;
  
   //
   for(int i = 0;i<SEM_ID_MAX;i++)
   {
       Blocked[i].current_Time = -1;
       Blocked[i].remainingTime = -1;
       Blocked[i].tid = -1;
       Blocked[i].tprio = -1;
       //Blocked[i].cond;
   }
   //
   for(int i = 0;i<SEM_ID_MAX;i++)
   {
       Ready[i].current_Time = -1;
       Ready[i].remainingTime = -1;
       Ready[i].tid = -1;
       Ready[i].tprio = -1;
       //Blocked[i].cond;
   }
   //
   running.current_Time =(float)-1.0;
   running.remainingTime =-1;
   running.tid = -1;
   running.tprio = -1;  
   //running.cond = pthread_cond_init();
   }
 
int schedule_me(float currentTime, int tid, int remainingTime, int tprio ) {
    int newTid = 0;
  Node newThread;
  Node temp;  // for remtime==0 cases
  
      newThread.current_Time = 0;
      newThread.tid = 0;
      newThread.remainingTime = 0;
      newThread.tprio = 0;
  if (find_tid(tid) == 0)
  {
      newThread.current_Time = currentTime;
      newThread.tid = tid;
      newThread.remainingTime = remainingTime;
      newThread.tprio = tprio;
      int newTid = 1;
  }
 g_currentTime = ceil(currentTime);// currentTime;// always
       if (typesched == FCFS)
       {
       if(lock ==1)
           {
           if(running.tid !=tid && tid_in_blocked(Blocked,tid)==0 && newThread.tid!=0) {//if(newTid == 0 && tid_in_blocked(&BlockedHead,tid) != 1 && tid != running.tid)//checks if we already have tid and id isnt already queued
          
               lockFCFS(tid);
           }
           else
             {    
               running.current_Time = currentTime;
               running.tid = tid;
               running.remainingTime = remainingTime;
               running.tprio = tprio;
               if(running.remainingTime == 0 )
                   {  
                       running = pop_blocked(Blocked);
                       unlockFCFS();      
                   }
             }
            return g_currentTime;
            }
          else if (lock == 0)
           {
           if (running.tid <=-1 || running.tid ==0)
           {
               running.current_Time = currentTime;
               running.tid = tid;
               running.remainingTime = remainingTime;
               running.tprio = tprio;
               lock=1;
               return g_currentTime;
           }
           else if (tid !=running.tid && tid_in_blocked(Blocked,tid)==0)
           {
               if(newThread.tid!=0)
               running = pop_blocked(Blocked);
               lockFCFS(tid); 
           }
       else if( running.tid == tid)
           {
               running.remainingTime == remainingTime;
               lock = 1;  
           }
       return g_currentTime;
         }
       }
   else if (typesched ==SRTF)
   {
          if (lock == 1){
       if(running.tid !=tid && tid_in_blocked(Blocked,tid)==0 && tid_in_ready(Blocked,tid)!=0 && newThread.tid!=0 && running.remainingTime < newThread.remainingTime && running.remainingTime > 0)
       {
          //printf("1\n");
          push_blocked(Blocked, newThread);
         //sortbyRemainingTime(Blocked);
          lockSRTF(tid);  
       }
       else if (running.tid !=tid && tid_in_blocked(Blocked,tid)==0 && newThread.tid!=0 && tid_in_ready(Blocked,tid)!=0 && newThread.remainingTime <= running.remainingTime)
       {
          //printf("2\n");
          temp = running;
          push_ready(Ready, temp); 
          running.current_Time = currentTime;
          running.tid = tid;
          running.remainingTime = remainingTime;
          running.tprio = tprio;
          unlockSRTF();
 
          if(running.remainingTime == 0 ){
          //   printf("3\n");
             sortbyRemainingTime(Ready);
            
           int value = Bcount;
           for(int i = 0; i<value;i++)
           {
           temp = pop_blocked(Blocked);
           push_ready(Ready,temp);
           }
           sortbyRemainingTime(Ready);
             running = pop_ready(Ready);
                 //printf("new thread running: tid-(%d) RemainTime-(%d) gTime- (%d) lock:(%d)\n", running.tid, running.remainingTime, g_currentTime,lock);
                 //printf("unlocked\n");
           //    unlockSRTF();
                  }
               }
               else
               {
               running.current_Time = currentTime;
               running.tid = tid;
               running.remainingTime = remainingTime;
               running.tprio = tprio;
              
               if(running.remainingTime==0)
               {
                   int value = Bcount;
               for(int i = 0; i<value;i++)
                   {
                       temp = pop_blocked(Blocked);
                       push_ready(Ready,temp);
                   }
                       sortbyRemainingTime(Ready);
                       running = pop_ready(Ready);
               //          printf("new thread running: tid-(%d) RemainTime-(%d) gTime- (%d) lock:(%d)\n", running.tid, running.remainingTime, g_currentTime,lock);
             //            printf("unlocked\n");
                       unlockSRTF();
               }
           }   
                return g_currentTime;
                 }
               else if(lock == 0)
               {
                   if (running.tid ==-1 || running.tid ==0)
                   {
                   running.current_Time = currentTime;
                   running.tid = tid;
                   running.remainingTime = remainingTime;
                   running.tprio = tprio;
                   lock = 1;
                   //lockSRTF(tid);
                   return g_currentTime;
                   }
               else if (tid !=running.tid && tid_in_blocked(Blocked,tid)==0 && newTid !=0 && tid_in_ready(Blocked,tid)!=0)//tid_in_blocked(&BlockedHead,tid)==1)// maybe able to remove first part
           {
               //if(newThread.tid!=0)
               //running = pop_blocked(Blocked);
               //lockSRTF(tid);
               push_ready(Ready,newThread);
           //  printf("5:running: tid-(%d) RemainTime-(%d) gTime- (%d) lock:(%d)\n", running.tid, running.remainingTime, g_currentTime,lock);
               return g_currentTime;
           }
                   if (tid == running.tid)
                   {
                      
                   running.remainingTime = remainingTime;
                  
                      lockSRTF(tid);
                   }
                   return g_currentTime;
               }
              return g_currentTime;
           }
       else if (typesched ==PBS)
       {
             if (lock == 1){
       if(running.tid !=tid && tid_in_blocked(Blocked,tid)==0 && tid_in_ready(Blocked,tid)!=0 && newThread.tid!=0 && running.tprio < newThread.tprio && running.remainingTime > 0)
       {
          //printf("1\n");
          push_blocked(Blocked, newThread);
         //sortbyRemainingTime(Blocked);
          lockPBS(tid);  
       }
       else if (running.tid !=tid && tid_in_blocked(Blocked,tid)==0 && newThread.tid!=0 && tid_in_ready(Blocked,tid)!=0 &&  newThread.tprio < running.tprio)
       {
          temp = running;
          push_ready(Ready, temp); 
          running.current_Time = currentTime;
          running.tid = tid;
          running.remainingTime = remainingTime;
          running.tprio = tprio;
          unlockPBS();
 
          if(running.remainingTime == 0 ){
        
             sortbyRemainingTime(Ready);
            
           int value = Bcount;
           for(int i = 0; i<value;i++)
           {
           temp = pop_blocked(Blocked);
           push_ready(Ready,temp);
           }
           sortbyRemainingTime(Ready);
             running = pop_ready(Ready);
               
                  }
               }
               else
               {
               running.current_Time = currentTime;
               running.tid = tid;
               running.remainingTime = remainingTime;
               running.tprio = tprio;
              
               if(running.remainingTime==0)
               {
                   int value = Bcount;
               for(int i = 0; i<value;i++)
                   {
                       temp = pop_blocked(Blocked);
                       push_ready(Ready,temp);
                   }
                       sortbyRemainingTime(Ready);
                       running = pop_ready(Ready);
 
                       unlockPBS();
                       }
                   }   
                return g_currentTime;
                 }
               else if(lock == 0)
               {
                   if (running.tid ==-1 || running.tid ==0)
                   {
                   running.current_Time = currentTime;
                   running.tid = tid;
                   running.remainingTime = remainingTime;
                   running.tprio = tprio;
                   lock = 1;
                   return g_currentTime;
                   }
               else if (tid !=running.tid && tid_in_blocked(Blocked,tid)==0 && newTid !=0 && tid_in_ready(Blocked,tid)!=0)//tid_in_blocked(&BlockedHead,tid)==1)// maybe able to remove first part
           {
               //if(newThread.tid!=0)
               //running = pop_blocked(Blocked);
               //lockSRTF(tid);
               push_ready(Ready,newThread);
           //  printf("5:running: tid-(%d) RemainTime-(%d) gTime- (%d) lock:(%d)\n", running.tid, running.remainingTime, g_currentTime,lock);
               return g_currentTime;
           }
                   if (tid == running.tid)
                   {
                      
                   running.remainingTime = remainingTime;
                  
                      lockSRTF(tid);
                   }
                   return g_currentTime;
               }
              return g_currentTime;
       }
          
       // has to be MLFQ for last case
       else
           {
        if(lock ==1)
           {
           if(running.tid !=tid && tid_in_blocked(Blocked,tid)==0 && newThread.tid!=0 && tid_in_ready(Ready,tid) ==0) {//if(newTid == 0 && tid_in_blocked(&BlockedHead,tid) != 1 && tid != running.tid)//checks if we already have tid and id isnt already queued
          
               lockPBS(tid);
           }
           else
             {    
               running.current_Time = currentTime;
               running.tid = tid;
               running.remainingTime = remainingTime;
               running.tprio = tprio;
               MLFQ_count +=1;
               if(running.remainingTime == 0 || MLFQ_count )
                   {  
                       running = pop_ready(Ready);
                           int value = Bcount;
               for(int i = 0; i<value;i++)
                   {
                       temp = pop_blocked(Blocked);
                       push_ready(Ready,temp);
                   }
                       //sortbyRemainingTime(Ready);
                       running = pop_ready(Ready);
                       unlockMLFQ();
                       MLFQ_count=0;
                   }
             }
            return g_currentTime;
            }
          else if (lock == 0)
           {
           if (running.tid <=-1 || running.tid ==0)
           {
               running.current_Time = currentTime;
               running.tid = tid;
               running.remainingTime = remainingTime;
               running.tprio = tprio;
               lock=1;
               MLFQ_count+=1;
               return g_currentTime;
           }
           else if (tid !=running.tid && tid_in_blocked(Blocked,tid)==0 && tid_in_ready(Ready,tid) ==0)
           {
               if(newThread.tid!=0)
               running = pop_blocked(Blocked);
               lockFCFS(tid); 
           }
       else if( running.tid == tid)
           {
               running.remainingTime == remainingTime;
               lock = 1;  
           }
       return g_currentTime;
         }
       }                  
}
typedef struct {
  int value; //decides switch, neg: go to blocked, pos: inc
  int tid;
  int current_Time;
  int remainingTime;
  int tprio;
  int sem_id;
  int senderTid;
} Semaphore;
 
Semaphore sems[SEM_ID_MAX - 1];
 
//this is where the block queue comes into play
int P( float currentTime, int tid, int sem_id) { // returns current global time
sems[sem_id].value--;
  if ( sems[sem_id].value < 0 ){
     sems[sem_id].tid = running.tid;
     sems[sem_id].remainingTime = running.remainingTime;
     sems[sem_id].current_Time = running.current_Time;
     sems[sem_id].tprio = running.tprio;
     sems[sem_id].sem_id = sem_id;
     sems[sem_id].senderTid = tid;
 
     pthread_cond_wait(&cond,&mutex);
    
     push_blocked(Blocked, running);
     Bcount++;
     running = pop_ready(Ready);
     Rcount--;
     }
 
return g_currentTime;
}
//semaphore struct probably needed to store who called semaphore
// and who locked them
int V( float currentTime, int tid, int sem_id){ // returns current global time
sems[sem_id].value++;
  Node temp;
  if ( sems[sem_id].value >= 0 ){
     int getTid = sems[sem_id].tid;
 
     pthread_cond_signal(&cond);//pthread_cond_broadcast(&cond);
 
     temp = pop_block_id(Blocked, getTid);
     Bcount--;
     push_ready(Ready, temp);
     Rcount++;
  }
 
return g_currentTime;
}
 
 
