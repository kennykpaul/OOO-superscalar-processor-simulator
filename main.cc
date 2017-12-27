#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <bitset>


#define IF 0
#define ID 1
#define IS 2
#define EX 3
#define WB 4

#define READY 1
#define NOT_READY 0


typedef struct _instruction_details{
int stage;
int pc;
int tag;
int type;
int src1_ready,src2_ready;
int dest_value,src1_value,src2_value;
int dest_reg,src1_reg,src2_reg;
int func_lat;
struct _instruction_details * next;
}ins_d;

typedef struct _register_file {
	int ready;
        int value;
}register_file;


typedef struct _inst_cache {
        int ins_pc;
	int ins_type;
	int ins_dest_reg;
	int ins_src1_reg;
	int ins_src2_reg;
        int ins_func_lat;
        int ins_tag;
        char  ins_mem_addr[9];
}inst_cache;

typedef struct _timing_for_stage {
	int tag;
	int fetch_start_cycle;
        int fetch_duration;
	int dispatch_start_cycle;
        int dispatch_duration;
	int issue_start_cycle;
        int issue_duration;
	int execute_start_cycle;
        int execute_duration;
	int writeback_start_cycle;
        int writeback_duration;
}timing_stage;

int proc_cycles=0;

int s=0,n=0,bs=0,lps=0,lpa=0,lss=0,lsa=0;

ins_d * fake_rob_head = 0;
ins_d * dispatch_queue_head = 0;
ins_d * issue_queue_head = 0;
ins_d * execute_queue_head = 0;

ins_d * fake_rob_end = 0;
ins_d * dispatch_queue_end = 0;
ins_d * issue_queue_end = 0;
ins_d * execute_queue_end = 0;

inst_cache ins_cache[20000];

int dispatch_count = 0;
int issue_count = 0;
int execute_count =0;


register_file  r_file[128];
timing_stage *timing=0;
const ins_d ins_d_init={-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,NULL};
const timing_stage timing_stage_init= {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};



void copy_queues_units(ins_d * ins_dest, ins_d * ins_src )
{
ins_dest->stage = ins_src->stage;
ins_dest->pc = ins_src->pc;
ins_dest->tag = ins_src->tag;
ins_dest->type = ins_src->type;
ins_dest->src1_ready = ins_src->src1_ready;
ins_dest->src2_ready = ins_src->src2_ready;
ins_dest->dest_value = ins_src->dest_value;
ins_dest->src1_value = ins_src->src1_value;
ins_dest->src2_value = ins_src->src2_value;
ins_dest->dest_reg = ins_src->dest_reg;
ins_dest->src1_reg = ins_src->src1_reg;
ins_dest->src2_reg = ins_src->src2_reg;
ins_dest->func_lat = ins_src->func_lat;
}


unsigned int bintohex(char *digits){
  unsigned int res=0;
  while(*digits)
    res = (res<<1)|(*digits++ -'0');
  return res;
}


bool char_equal(char* a, char* b, int N){
  int count=0;
  for(int i=0;i<N;++i)
  {
    if(a[i]==b[i])count++;
  }
  return count==N;
}

int get_setindex(char* addrBin, int tag_size, int index_size){
  int index_value=0;
 // printf("%s\n",addrBin);
  for(int i=0;i<index_size;++i){
    if(addrBin[tag_size+i]=='1')
    {
    index_value+= pow(2,index_size-i-1);
    }
  }
 // printf("%d\n", index_value);
  return index_value;
}

/*

struct cache_status
{
  int access_count;  
  int read_count; 
  int write_count; 
  int replacement_count;
  int miss_count;
  int hit_count;
  int read_miss_count;
  int write_miss_count; 
  int read_hit_count;
  int write_hit_count;
  int write_back_count; 
  double hit_rate;
  double miss_rate;
  int total_memory_traffic;
} status ={0,0,0,0,0,0,0,0,0,0,0,0,0,0};

void printstatus(int write_policy)
{
  
  status.hit_count=status.read_hit_count + status.write_hit_count;
  status.hit_rate=(double) status.hit_count/status.access_count;
  status.miss_rate=(double) status.miss_count/status.access_count;
  if(!write_policy)
  status.total_memory_traffic=status.read_miss_count+status.write_miss_count+status.write_back_count;
  else
  status.total_memory_traffic=status.read_miss_count+status.write_count+status.write_back_count;
 
  printf("======  Simulation results (raw) ======\n");
  printf("a. number of L1 reads:%d\n",status.read_count);
  printf("b. number of L1 read misses:%d\n",status.read_miss_count);
  printf("c. number of L1 writes:%d\n", status.write_count);
  printf("d. number of L1 write misses:%d\n", status.write_miss_count);
  printf("e. L1 miss rate: %.4f \n", status.miss_rate);
  printf("f. number of writebacks from L1:%d\n",status.write_back_count);
  printf("g. total memory traffic: %d\n",status.total_memory_traffic);


}

*/

class block 
{
public:
      char tag[32];
      unsigned long timestamp;
      bool valid;
      bool dirty;
      int block_recency_count;
      double block_freq_count;
      double temp_freq_count;
      double block_aging_count;
      int last_count;
      int current_count;
      int hit;
block(int tag_size)
{
    
 for(int i=0;i<tag_size;++i)
   tag[i]='0';//initialize tag to random chars

 valid=false;
 dirty=false;
 block_recency_count=0;
 block_freq_count=0;
 block_aging_count=0;
 last_count=0;
 current_count=0;
 hit=0;
}

};


class set {
public:
      block* blocks[256];
      int set_associativity;
      float lamda;

set(int set_associativity,int tag_size)
{
  for(int i=0;i<set_associativity;++i)
  {
 //   printf("going to create block no. %d\n",i);
    blocks[i]=new block(tag_size); 
   // printf("block tag is %s\n", blocks[i]->tag);
   // printf("The block valid is %d\n",blocks[i]->valid);
  }
}  

void set_hit(int location)
{
blocks[location]->hit=1;
}

void clear_hit(int location)
{
blocks[location]->hit=0;
}

void set_valid(int location)
{
  blocks[location]->valid=true;
}

void set_current_count(int location, int count)
{
  blocks[location]->current_count=count;
}

void set_last_count(int location, int count)
{
  blocks[location]->last_count=count;
}

void set_age_count(int location)
{
  blocks[location]->block_aging_count=blocks[location]->block_freq_count;
}
/*
char get_tag(int location)
{
return blocks[location]->tag;
}
*/
void set_dirty(int location)
{
  blocks[location]->dirty = true;
}

void clear_dirty(int location)
{
blocks[location]->dirty = false;
}

int ishit(char* addrBin, int tag_size)
{
  for(int j=0;j<set_associativity;++j)
  {
    if(char_equal(blocks[j]->tag, addrBin, tag_size) && blocks[j]->valid)
    {
//	  printf("hit on set %d line %d\n",setIndex, j);
	  return j;
    }
  }
    //Give signal to processor or lower level cache that it is a miss
    return -1;
}

int emptyLineAvailable()
{
//printf("Entering %d\n",set_associativity);
  for(int j=0;j<set_associativity;++j)
  {
//  printf("The block valid is %d\n",blocks[j]->valid);
     if (blocks[j]->valid==false)
{ 
    // printf("Yes");
     return j;
} 
 }
    return -1;
}


bool check_dirty(int location)
{
    if (blocks[location]->dirty==true) 
      return true;
    else
      return false;
}


void update_recency_count(int location)
{
  if(blocks[location]->valid)
  {
    for(int i=0;i<set_associativity;++i)
    {
      if(blocks[i]->valid && (blocks[i]->block_recency_count < blocks[location]->block_recency_count))
      {
	blocks[i]->block_recency_count++;
//	blocks[i]->block_recency_count = blocks[i]->block_recency_count% set_associativity;
      }
    }
  }
  else
  {
    for(int i=0;i<set_associativity;++i)
    {
     if(blocks[i]->valid)
      {
        blocks[i]->block_recency_count++;
      }
    }
  }
blocks[location]->block_recency_count=0;//clear the age of hitted line
}  


void update_freq_count(int location)
{
if(lamda>1)
blocks[location]->block_freq_count++;
else
{
if(blocks[location]->hit==1)
{
blocks[location]->block_freq_count=(1+(pow(0.5,(lamda*(blocks[location]->current_count-blocks[location]->last_count)))*blocks[location]->block_freq_count));
//printf(" block freq count %f\n",blocks[location]->block_freq_count);
//printf(" prev count %d %d \n", blocks[location]->last_count,location);
//printf(" current count %d \n", blocks[location]->current_count);
}
else
{
blocks[location]->block_freq_count=1;
//printf(" block freq count %f\n",blocks[location]->block_freq_count);
//printf(" prev count %d %d \n", blocks[location]->last_count,location);
//printf(" current count %d \n", blocks[location]->current_count);
}
}
}

void set_tag(char* addrBin, int location, int tag_size)
{ 
    
    for(int i=0;i<tag_size;++i) 
      blocks[location]->tag[i]=addrBin[i];
    //printf("tag is %s \n", block[location]->tag);
//    status.stream_in_count++;
}

char* get_tag_index(int setindex,int location,int tag_size,int index_size)
{
   char* tag=new char[33];
   int rem,i;
   for(int i=0;i<tag_size;++i)
    tag[i]=blocks[location]->tag[i];
   char binaryNumber[32];
   for(int i=0;i<index_size;++i)
     binaryNumber[i]='0';
   for(i = 0; i < index_size; i++)
   { 
      rem = setindex % 2;
      if(rem == 0)
        binaryNumber[index_size-1-i]='0';
      else
        binaryNumber[index_size-1-i]='1';
      setindex=setindex/2;
   }
   binaryNumber[index_size]='\0';

   for(int i=tag_size;i<tag_size+index_size;++i)
     tag[i]=binaryNumber[i-(tag_size)];
   for(int i=tag_size+index_size;i<32;++i)
     tag[i]='0';
   tag[33]='\0';
   return tag;
}


int get_lru_eviction_spot()
{ 
  int location=0;
  for(int i=0;i<set_associativity;++i)
  { 
    if((blocks[i]->block_recency_count >= blocks[location]->block_recency_count))
    {
      location=i;
    }
  }  
return location;
}

int get_lfu_eviction_spot()
{ 
  int location=0;

  if(lamda>1)
  {
  for(int i=set_associativity-1;i>=0;--i)
  { 
    if((blocks[i]->block_freq_count <= blocks[location]->block_freq_count))
    {
      location=i;
    }
  }  
}

else
{
for(int i=set_associativity-1;i>=0;--i)
{
 double temp=pow(2,(lamda*(blocks[i]->last_count-blocks[i]->current_count)));
//printf("%.20f\n", temp);
blocks[i]->temp_freq_count=(double)(temp*blocks[i]->block_freq_count);
//printf(" %.100f  %d  %d\n",blocks[i]->temp_freq_count,blocks[i]->current_count,blocks[i]->last_count);
}

  for(int j=set_associativity-1;j>=0;--j)
  {
//printf("%.20f %.20f %d  ",blocks[j]->temp_freq_count,blocks[location]->temp_freq_count,location);
    if((blocks[j]->temp_freq_count) <= (blocks[location]->temp_freq_count))
    {
      location=j;
//     printf("%.20f %.20f %d  ",blocks[j]->temp_freq_count,blocks[location]->temp_freq_count,location);
    }
  }
//printf("   %d",location);
}
return location;

}
};
	
class cache {
public:
      int cache_size;
      int victim_cache_size;
      int victim_set_assoc;
      int victim_tag_size;
      int block_size;
      int set_associativity;
      int tag_size;
      int index_size;
      int num_sets;
      set* sets[65536];
      set* victim_set;
      int replacement_policy;
      int write_policy;

  int access_count;  
  int read_count; 
  int write_count; 
  int replacement_count;
  int miss_count;
  int hit_count;
  int read_miss_count;
  int write_miss_count; 
  int read_hit_count;
  int write_hit_count;
  int write_back_count; 
  double hit_rate;
  double miss_rate;
  int swaps;
  int victim_writebacks;
  int total_memory_traffic;
  float lamda;
void init_cache()
{
  num_sets=cache_size/(block_size*set_associativity);
  tag_size=(32-(int)log2(num_sets)-(int)log2(block_size));
  victim_tag_size=(32-(int)log2(block_size)); 
  index_size=(int)log2(num_sets);  
  victim_set_assoc=(victim_cache_size/block_size);
  //printf("num_sets %d\n", num_sets);
  //printf("tag_size %d\n", tag_size);
  //printf("index_size %d\n", index_size);
  //printf("victim tag size is %d\n",victim_tag_size);
  //printf("victim set associativity is %d\n",victim_set_assoc);
  access_count=0;
  read_count=0;
  write_count=0;
  replacement_count=0;
  miss_count=0;
  hit_count=0;
  read_miss_count=0;
  write_miss_count=0;
  read_hit_count=0;
  write_hit_count=0;
  write_back_count=0;
  hit_rate=0;
  miss_rate=0;
  swaps=0;
  victim_writebacks=0;  
  total_memory_traffic=0;
//  lamda=5;  
  for(int i=0;i<num_sets;++i) 
{
  // printf("going to create set no. %d\n",i);
    sets[i]= new set(set_associativity,tag_size);
    sets[i]->set_associativity=set_associativity;
    sets[i]->lamda=lamda;
}
victim_set = new set(victim_set_assoc,victim_tag_size);
victim_set->set_associativity=victim_set_assoc;
}
  
void printstatus(int write_policy)
{
  
  hit_count=read_hit_count + write_hit_count;
  hit_rate=(double) hit_count/access_count;
  miss_rate=(double) miss_count/access_count;
  if(!write_policy)
  total_memory_traffic=read_miss_count+write_miss_count+write_back_count;
  else
  total_memory_traffic=read_miss_count+write_count+write_back_count;
 
  printf("======  Simulation results (raw) ======\n");
  printf("a. number of L1 reads:%d\n",read_count);
  printf("b. number of L1 read misses:%d\n",read_miss_count);
  printf("c. number of L1 writes:%d\n", write_count);
  printf("d. number of L1 write misses:%d\n", write_miss_count);
  printf("e. L1 miss rate: %.4f \n", miss_rate);
//  printf("f. number of writebacks from L1:%d\n",write_back_count);
//  printf("g. total memory traffic: %d\n",total_memory_traffic);


}

void dump_parameter()
{
//printf("Cache size is %d\n", cache_size);
//printf("Block size is %d\n", block_size);
//printf("Set associativity is %d\n",set_associativity);
//printf("Replacement policy is %d\n", replacement_policy);
//printf("Write policy is %d\n", write_policy);
//printf("Victim cache size is %d\n",victim_cache_size);
//printf("lamda value is %f\n",lamda);
}

char* read(char* addrBin)
{   
    char* ret_addr;
    ret_addr = NULL;
  //  printf("#1: Read %x\n",bintohex(addrBin));
    int setindex = get_setindex(addrBin,tag_size,index_size);
   // printf("Current set    %d:\n", setindex);
    //printf("reading set %....\n ", setindex);
    read_count++;
    access_count++;
for(int j=0;j<set_associativity;j++)
{
sets[setindex]->set_current_count(j,access_count);
}
    int hit_location=sets[setindex]->ishit(addrBin,tag_size);
    if(hit_location!=-1)
    {
      read_hit_count++;
      //printf("L1 HIT\n");
      if(!replacement_policy)
      {
        // printf("L1 UPDATE LRU\n");
        //printf("Changed set     %d:    %x   D\n",setindex,bintohex(sets[setindex]->blocks[hit_location]->tag));
	sets[setindex]->update_recency_count(hit_location);
      }
      else
       {
     //printf("L1 UPDATE LFU\n");
       //printf("Changed set     %d:    %x   D\n",setindex,bintohex(sets[setindex]->blocks[hit_location]->tag));
        sets[setindex]->set_hit(hit_location);
        sets[setindex]->set_current_count(hit_location,access_count);
	sets[setindex]->update_freq_count(hit_location);
        sets[setindex]->set_last_count(hit_location,access_count);
        sets[setindex]->clear_hit(hit_location);
        
        //printf("%d",sets[setindex]->blocks[hit_locaiton]->freq_count);
       }
    } //if end
    else 
    { 
      read_miss_count++;
      miss_count++;
      //printf("L1 MISS\n");
      int empty_location=sets[setindex]->emptyLineAvailable();
     // printf("I am read\n");
     // printf("The empty location is %d\n",empty_location);
      if(empty_location!=-1)
      {
	//printf("R missed, space available, stream in \n");
	if(!replacement_policy)
	{  
         //printf("L1 UPDATE LRU\n");
//         printf("Changed set     %d:    D\n",setindex);
	  ret_addr=addrBin;         
	  sets[setindex]->set_tag(addrBin,empty_location,tag_size);
          //printf("Changed set     %d:    %x   D\n",setindex,bintohex(sets[setindex]->blocks[empty_location]->tag));
          sets[setindex]->update_recency_count(empty_location); 
	  sets[setindex]->set_valid(empty_location);
        }
	else
	{
        //printf("L1 UPDATE LFU\n");
        //printf("Changed set     %d:    D\n",setindex);
           ret_addr=addrBin;
	  sets[setindex]->set_tag(addrBin,empty_location,tag_size);
          //printf("Changed set     %d:    %x   D\n",setindex,bintohex(sets[setindex]->blocks[empty_location]->tag));
          sets[setindex]->set_current_count(empty_location,access_count);
          sets[setindex]->update_freq_count(empty_location);
          sets[setindex]->set_last_count(empty_location,access_count);
          sets[setindex]->set_valid(empty_location);
        }
      } //pending top else
      else 
      {
	//printf("R missed and full, do eviction\n");
	if(!replacement_policy)
	{
          //printf("L1 UPDATE LRU\n");
          if(victim_cache_size!=0)
	  {
	   // read_miss_count--;
            int temp=0;
            int location= sets[setindex]->get_lru_eviction_spot();
            int victim_hit_location=victim_set->ishit(addrBin,victim_tag_size);
            if(victim_hit_location!=-1)
            {
	      read_miss_count--;
	      swaps++;
              victim_set->set_tag((sets[setindex]->get_tag_index(setindex,location,tag_size,index_size)),victim_hit_location,victim_tag_size);
              victim_set->update_recency_count(victim_hit_location);
              if(victim_set->check_dirty(victim_hit_location))temp=1;
              if(sets[setindex]->check_dirty(location))victim_set->set_dirty(victim_hit_location);
              else victim_set->clear_dirty(victim_hit_location);
              if(temp)sets[setindex]->set_dirty(location);
              else sets[setindex]->clear_dirty(location);
            }
            else
            {
              int victim_empty_location=victim_set->emptyLineAvailable();
              if(victim_empty_location!=-1)
              {
                ret_addr=addrBin;
                victim_set->set_tag((sets[setindex]->get_tag_index(setindex,location,tag_size,index_size)),victim_empty_location,victim_tag_size);
                victim_set->update_recency_count(victim_empty_location);
                victim_set->set_valid(victim_empty_location);
                if(sets[setindex]->check_dirty(location))victim_set->set_dirty(victim_empty_location);
                sets[setindex]->clear_dirty(location);
              }
              else
              {
                int victim_location= victim_set->get_lru_eviction_spot();
                ret_addr=addrBin;
                if (victim_set->check_dirty(victim_location))
                {
		victim_writebacks++;
                 ret_addr=victim_set->get_tag_index(0,victim_location,victim_tag_size,0);
                 victim_set->clear_dirty(victim_location);
                }
                 victim_set->set_tag((sets[setindex]->get_tag_index(setindex,location,tag_size,index_size)),victim_location,victim_tag_size);
                 if(sets[setindex]->check_dirty(location))victim_set->set_dirty(victim_location);
                 sets[setindex]->clear_dirty(location);
                 victim_set->update_recency_count(victim_location);
              }
            }
          sets[setindex]->set_tag(addrBin,location,tag_size);
          sets[setindex]->update_recency_count(location);
	  }
          else
          {
          int location= sets[setindex]->get_lru_eviction_spot();
          ret_addr=addrBin;
	  if (sets[setindex]->check_dirty(location))//this is done only for wbwa
	  { 
            ret_addr=sets[setindex]->get_tag_index(setindex,location,tag_size,index_size);
//             printf("I am read %s at location %d\n",sets[setindex]->blocks[location]->tag,location);
            // printf("%s\n",ret_addr);
	    write_back_count++;
            sets[setindex]->clear_dirty(location);
	    //call the function which obtains the tag and provide higher level memory tag, set index and location to write
	  }
          //for wtna just remove the block and proceed with replacement of tag no updation is required
	  //update the cache data contents
	 // printf("Changed set     %d:     D\n",setindex);
	  sets[setindex]->set_tag(addrBin,location,tag_size);
          //printf("Changed set     %d:    %x   D\n",setindex,bintohex(sets[setindex]->blocks[location]->tag));
	  sets[setindex]->update_recency_count(location); 
          }     
        }
	else
  	{
          //printf("L1 UPDATE LFU\n");
           if(victim_cache_size!=0)
          {
//	    read_miss_count--;
            int temp=0;
            int location= sets[setindex]->get_lfu_eviction_spot();
            int victim_hit_location=victim_set->ishit(addrBin,victim_tag_size);
            if(victim_hit_location!=-1)
            {
		read_miss_count--;
		swaps++;
              victim_set->set_tag((sets[setindex]->get_tag_index(setindex,location,tag_size,index_size)),victim_hit_location,victim_tag_size);
              victim_set->update_recency_count(victim_hit_location);
              if(victim_set->check_dirty(victim_hit_location))temp=1;
              if(sets[setindex]->check_dirty(location))victim_set->set_dirty(victim_hit_location);
              else victim_set->clear_dirty(victim_hit_location);
              if(temp)sets[setindex]->set_dirty(location);
              else sets[setindex]->clear_dirty(location);
            }
            else
            {
              int victim_empty_location=victim_set->emptyLineAvailable();
              if(victim_empty_location!=-1)
              {
                ret_addr=addrBin;
                victim_set->set_tag((sets[setindex]->get_tag_index(setindex,location,tag_size,index_size)),victim_empty_location,victim_tag_size);
                victim_set->update_recency_count(victim_empty_location);
                victim_set->set_valid(victim_empty_location);
                if(sets[setindex]->check_dirty(location))victim_set->set_dirty(victim_empty_location);
                sets[setindex]->clear_dirty(location);
              }
              else
              {
                int victim_location= victim_set->get_lru_eviction_spot();
                ret_addr=addrBin;
                if (victim_set->check_dirty(victim_location))
                {
		 victim_writebacks++;
                 ret_addr=victim_set->get_tag_index(0,victim_location,victim_tag_size,0);
                 victim_set->clear_dirty(victim_location);
                }
                 victim_set->set_tag((sets[setindex]->get_tag_index(setindex,location,tag_size,index_size)),victim_location,victim_tag_size);
                 if(sets[setindex]->check_dirty(location))victim_set->set_dirty(victim_location);
                 sets[setindex]->clear_dirty(location);
	   //      victim_set->set_age_count(victim_location);
                 victim_set->update_recency_count(victim_location);
              }
            }
          sets[setindex]->set_tag(addrBin,location,tag_size);
          sets[setindex]->set_age_count(location);
          sets[setindex]->set_current_count(location,access_count);
          sets[setindex]->update_freq_count(location);
          sets[setindex]->set_last_count(location,access_count);

          }
          else
          { 
          int location= sets[setindex]->get_lfu_eviction_spot();
           ret_addr=addrBin;
	  if (sets[setindex]->check_dirty(location))
	  {
            ret_addr=sets[setindex]->get_tag_index(setindex,location,tag_size,index_size);
  //          printf("I am read %s at location %d\n",sets[setindex]->blocks[location]->tag,location);
            // printf("%s\n",ret_addr);
	    write_back_count++;
            sets[setindex]->clear_dirty(location);
	    //call the function which obtains the tag and provide higher level memory tag, set index and location to write
	  }
//	printf("Changed set     %d:    D\n",setindex);
        sets[setindex]->set_tag(addrBin,location,tag_size);
        //printf("Changed set     %d:    %x   D\n",setindex,bintohex(sets[setindex]->blocks[location]->tag));
	sets[setindex]->set_age_count(location);
        sets[setindex]->set_current_count(location,access_count);
	sets[setindex]->update_freq_count(location);
        sets[setindex]->set_last_count(location,access_count);
          }	
        }
      }
   }
/*
for(int j=0;j<set_associativity;j++)
{
if(sets[setindex]->blocks[j]->dirty)
printf("  %x   D  %f  %d %d",bintohex(sets[setindex]->blocks[j]->tag),sets[setindex]->blocks[j]->block_freq_count, sets[setindex]->blocks[j]->current_count, sets[setindex]->blocks[j]->last_count);
else
printf("  %x      %f %d %d",bintohex(sets[setindex]->blocks[j]->tag),sets[setindex]->blocks[j]->block_freq_count, sets[setindex]->blocks[j]->current_count, sets[setindex]->blocks[j]->last_count);
}
printf("\n");
*/

/*
for(int j=0;j<vicset_assoc;j++)
{
if(victim_set->blocks[j]->dirty)
printf("  %x   D %f   ",bintohex(sets->blocks[j]->tag),sets->blocks[j]->block_freq_count);
else
printf("  %x %f ",bintohex(sets->blocks[j]->tag),sets->blocks[j]->block_freq_count);
}
printf("\n");
*/
return ret_addr;

}

char* write(char* addrBin)
{
    char* ret_addr;
    ret_addr = NULL;
   // printf("#1: Write %x\n",bintohex(addrBin));
    int setindex = get_setindex(addrBin,tag_size,index_size);
//   printf("%X\n",bintohex(addrBin));
  //  printf("Current set    %d:\n", setindex);
    write_count++;
    access_count++;
for(int j=0;j<set_associativity;j++)
{
sets[setindex]->set_current_count(j,access_count);
}
   //printf("I am write\n");
    int hit_location=sets[setindex]->ishit(addrBin,tag_size);
    //printf("L1 Write:%s(tag %p, index %d\n)",addrBin,sets[setindex]->blocks[hit_location]->tag,setindex);
    //printf("Current set    %d:\n", setindex);
    if(hit_location!=-1)
    {
      write_hit_count++;
      //printf("L1 HIT\n");
      if(!replacement_policy)
      {
        //printf("L1 UPDATE LRU\n");
	sets[setindex]->update_recency_count(hit_location);
        if(!write_policy)
        {//update the cache data contents for wbwa
	  //printf("L1 SET DIRTY\n");
          //printf("Changed set     %d:    %x   D\n",setindex,bintohex(sets[setindex]->blocks[hit_location]->tag));
       //  sets[setindex]->update_recency_count(hit_location);
	  sets[setindex]->set_dirty(hit_location);
        }
        else
        {
         ;//update the cache data contents
          //call the function which provide addrbin to higher level memory for updation for wtna
	}
      }
      else
      {
      //  printf("L1 UPDATE LFU\n");
        sets[setindex]->set_hit(hit_location);
        sets[setindex]->set_current_count(hit_location,access_count);
	sets[setindex]->update_freq_count(hit_location);
        sets[setindex]->set_last_count(hit_location,access_count);
        sets[setindex]->clear_hit(hit_location);
        if(!write_policy)
        {//update the cache data contents
	//  printf("L1 SET DIRTY\n");
          //printf("Changed set     %d:    %X   D\n",setindex,bintohex(sets[setindex]->blocks[hit_location]->tag));
         // sets[setindex]->update_freq_count(hit_location);
	  sets[setindex]->set_dirty(hit_location);
        }
        else
        {
         ; //update the cache data contents
           //call the function which provide addrbin to higher level memory for updation for wtna
	}
      }
    } 
    else 
    { 
      write_miss_count++;
      miss_count++;
      //printf("L1 MISS\n");
      int empty_location=sets[setindex]->emptyLineAvailable();
      //printf("Changed set     %d:    D\n",setindex);
//      printf("location is %d", empty_location);
      if(!write_policy)//for wbwa
      {
        if(empty_location!=-1)
        {
//	  printf("W missed, space available, stream in \n");
	  if(!replacement_policy)
	  {
            ret_addr=addrBin;
            //printf("L1 UPDATE LRU\n");
	    sets[setindex]->set_tag(addrBin,empty_location,tag_size);
	    //update the cache data contents
            sets[setindex]->update_recency_count(empty_location); 
	    sets[setindex]->set_valid(empty_location);
	    //printf("L1 SET DIRTY\n");
            //printf("Changed set     %d:    %x   D\n",setindex,bintohex(sets[setindex]->blocks[empty_location]->tag));
	    sets[setindex]->set_dirty(empty_location);
          }
	  else
	  {
            ret_addr=addrBin;
        //    printf("L1 UPDATE LFU\n");
	    sets[setindex]->set_tag(addrBin,empty_location,tag_size);
	    //update the cache contents
	    sets[setindex]->set_current_count(empty_location,access_count);
            sets[setindex]->update_freq_count(empty_location);
            sets[setindex]->set_last_count(empty_location,access_count);
            sets[setindex]->set_valid(empty_location);
	  //  printf("L1 SET DIRTY\n");
            //printf("Changed set     %d:    %X   D\n",setindex,bintohex(sets[setindex]->blocks[empty_location]->tag));
            sets[setindex]->set_dirty(empty_location);
          }
        } 
        else 
        {
//	  printf("W missed and full, do eviction\n");
	  if(!replacement_policy)
	  {
          if(victim_cache_size!=0)
          { 
	  //  write_miss_count--;
            int temp=0;
            int location= sets[setindex]->get_lru_eviction_spot();
            int victim_hit_location=victim_set->ishit(addrBin,victim_tag_size);
            if(victim_hit_location!=-1)
            {
		write_miss_count--;
		swaps++;
              victim_set->set_tag((sets[setindex]->get_tag_index(setindex,location,tag_size,index_size)),victim_hit_location,victim_tag_size);
              victim_set->update_recency_count(victim_hit_location);
              if(victim_set->check_dirty(victim_hit_location))temp=1;
              if(sets[setindex]->check_dirty(location))victim_set->set_dirty(victim_hit_location);
              else victim_set->clear_dirty(victim_hit_location);
              if(temp)sets[setindex]->set_dirty(location);
              else sets[setindex]->clear_dirty(location);
            }
            else
            {
              int victim_empty_location=victim_set->emptyLineAvailable();
              if(victim_empty_location!=-1)
              {
                ret_addr=addrBin;
                victim_set->set_tag((sets[setindex]->get_tag_index(setindex,location,tag_size,index_size)),victim_empty_location,victim_tag_size);
                victim_set->update_recency_count(victim_empty_location);
                victim_set->set_valid(victim_empty_location);
                if(sets[setindex]->check_dirty(location))victim_set->set_dirty(victim_empty_location);
                sets[setindex]->clear_dirty(location);
              }
              else
              {
                int victim_location= victim_set->get_lru_eviction_spot();
                ret_addr=addrBin;
                if (victim_set->check_dirty(victim_location))
                {
		victim_writebacks++;
                 ret_addr=victim_set->get_tag_index(0,victim_location,victim_tag_size,0);
                 victim_set->clear_dirty(victim_location);
                }
                 victim_set->set_tag((sets[setindex]->get_tag_index(setindex,location,tag_size,index_size)),victim_location,victim_tag_size);
                 victim_set->update_recency_count(victim_location);
                 if(sets[setindex]->check_dirty(location))victim_set->set_dirty(victim_location);
                 sets[setindex]->clear_dirty(location);
              }
            }
          sets[setindex]->set_tag(addrBin,location,tag_size);
          sets[setindex]->update_recency_count(location);
	  sets[setindex]->set_dirty(location);
          }
           else
           { 
	    //printf("L1 UPDATE LRU\n");
            int location= sets[setindex]->get_lru_eviction_spot();
             ret_addr=addrBin;
	    if (sets[setindex]->check_dirty(location))
	    {
               ret_addr=sets[setindex]->get_tag_index(setindex,location,tag_size,index_size);
           //     printf("I am write %s at location %d\n",sets[setindex]->blocks[location]->tag,location);
              // printf("%s\n",ret_addr);
	      write_back_count++;
	    //call the function which obtains the tag and provide higher level memory tag, set index and location to write
	    }
	    sets[setindex]->set_tag(addrBin,location,tag_size);
            //update the cache data contents
            //printf("L1 SET DIRTY\n");
            //printf("Changed set     %d:    %x   D\n",setindex,bintohex(sets[setindex]->blocks[location]->tag));
	    sets[setindex]->update_recency_count(location);
            sets[setindex]->set_dirty(location);
            }
          }
	  else
  	  {
           if(victim_cache_size!=0)
          { 
	   // write_miss_count--;
            int temp=0;
            int location= sets[setindex]->get_lfu_eviction_spot();
            int victim_hit_location=victim_set->ishit(addrBin,victim_tag_size);
            if(victim_hit_location!=-1)
            {
		write_miss_count--;
		swaps++;
              victim_set->set_tag((sets[setindex]->get_tag_index(setindex,location,tag_size,index_size)),victim_hit_location,victim_tag_size);
              victim_set->update_recency_count(victim_hit_location);
              if(victim_set->check_dirty(victim_hit_location))temp=1;
              if(sets[setindex]->check_dirty(location))victim_set->set_dirty(victim_hit_location);
              else victim_set->clear_dirty(victim_hit_location);
              if(temp)sets[setindex]->set_dirty(location);
              else sets[setindex]->clear_dirty(location);
            }
            else
            {
              int victim_empty_location=victim_set->emptyLineAvailable();
              if(victim_empty_location!=-1)
              {
                ret_addr=addrBin;
                victim_set->set_tag((sets[setindex]->get_tag_index(setindex,location,tag_size,index_size)),victim_empty_location,victim_tag_size);
                victim_set->update_recency_count(victim_empty_location);
                victim_set->set_valid(victim_empty_location);
                if(sets[setindex]->check_dirty(location))victim_set->set_dirty(victim_empty_location);
                sets[setindex]->clear_dirty(location);
              }
              else
              {
                int victim_location= victim_set->get_lru_eviction_spot();
                ret_addr=addrBin;
                if (victim_set->check_dirty(victim_location))
                {
		 victim_writebacks++;
                 ret_addr=victim_set->get_tag_index(0,victim_location,victim_tag_size,0);
                 victim_set->clear_dirty(victim_location);
                }
                 victim_set->set_tag((sets[setindex]->get_tag_index(setindex,location,tag_size,index_size)),victim_location,victim_tag_size);
   //              victim_set->set_age_count(victim_location);
                 victim_set->update_recency_count(victim_location);
                 if(sets[setindex]->check_dirty(location))victim_set->set_dirty(victim_location);
                 sets[setindex]->clear_dirty(location);
              }
            }
          sets[setindex]->set_tag(addrBin,location,tag_size);
          sets[setindex]->set_age_count(location);
          sets[setindex]->set_current_count(location,access_count);
	  sets[setindex]->update_freq_count(location);
          sets[setindex]->set_last_count(location,access_count);
	  sets[setindex]->set_dirty(location);
          }
            else
            {
            //printf("L1 UPDATE LFU\n");
            int location= sets[setindex]->get_lfu_eviction_spot();
             ret_addr=addrBin;
	    if (sets[setindex]->check_dirty(location))
	    {
              ret_addr=sets[setindex]->get_tag_index(setindex,location,tag_size,index_size);
            //  printf("I am write %s at location %d\n",sets[setindex]->blocks[location]->tag,location);
	     // printf("%s\n",ret_addr);
	      write_back_count++;
	      //call the function which obtains the tag and provide higher level memory tag, set index and location to write
	    }
	    sets[setindex]->set_tag(addrBin,location,tag_size);
            //update the cache data contents
            //printf("L1 SET DIRTY\n");
            //printf("Changed set     %d:    %x   D\n",setindex,bintohex(sets[setindex]->blocks[location]->tag));
	    sets[setindex]->set_age_count(location);
            sets[setindex]->set_current_count(location,access_count);
	    sets[setindex]->update_freq_count(location);
            sets[setindex]->set_last_count(location,access_count);
            sets[setindex]->set_dirty(location);
            }	
          }
        }
      }
      else
      {
      ;//call the function which provide addrbin to higher level memory for updation
      }
   }
/*
for(int j=0;j<set_associativity;j++)
{
if(sets[setindex]->blocks[j]->dirty)
printf("  %x   D  %f %d %d",bintohex(sets[setindex]->blocks[j]->tag),sets[setindex]->blocks[j]->block_freq_count, sets[setindex]->blocks[j]->current_count, sets[setindex]->blocks[j]->last_count);
else
printf("  %x      %f %d %d",bintohex(sets[setindex]->blocks[j]->tag),sets[setindex]->blocks[j]->block_freq_count, sets[setindex]->blocks[j]->current_count, sets[setindex]->blocks[j]->last_count);
}
printf("\n");
*/

/*
for(int j=0;j<victim_set_assoc;j++)
{
if(victim_set->blocks[j]->dirty)
printf("  %x   D %f  ",bintohex(sets->blocks[j]->tag),sets->blocks[j]->block_freq_count);
else
printf("  %x %f      ",bintohex(sets->blocks[j]->tag),sets->blocks[j]->block_freq_count);
}
printf("\n");
*/
return ret_addr;

}      
    
void set_cache_param(int param, int value)
{
  switch (param) 
   {
     case 1:
      block_size = value;
//      printf("did");
     break;
     case 2:
      cache_size = value;
//      printf("did");
     break;
     case 3:
      set_associativity = value;
//      printf("did");
     break;
     case 4:
      replacement_policy = value;
//      printf(value);
     break;
     case 5:
       write_policy = value;
     break;
     case 6:
       victim_cache_size=value;
     break;
     default:
       printf("error set_cache_param: bad parameter value\n");
       exit(-1);
  }
}

};

int is_power_of_2(int i) {
    if ( i <= 0 ) 
    {
        return 0;
    }
    return ! (i & (i-1));
}

bool parameter_sanity_check( char* argv[])
{
int arg=0;
float value=0;
float temp=0;
for(arg=1; arg<9; arg++)
{
     if(arg==1) 
     {
        printf("  L1_BLOCKSIZE:    %s\n",argv[arg]);
	value = atoi(argv[arg]);
        if(is_power_of_2(value)==0)
        {
	  printf("The cache block size is not power of two\n");
	  return false;
        }
        continue;
     }
     if(arg==2)
     {
	printf("  L1_SIZE:     %s\n",argv[arg]);
	value = atoi(argv[arg]);
        if(is_power_of_2(value)==0)
        {
	  printf("The cache size is not power of two\n");
          return false;
        } 
        continue;
     }
 
    if(arg==3) 
    {
	printf("  L1_ASSOC:     %s\n",argv[arg]);
	value = atoi(argv[arg]);
        continue;
    }
/*
    if(arg==4) 
    {
	if(*argv[arg]==0)
        {
	  printf("  L1_REPLACEMENT_POLICY:   %s\n",argv[arg]);
        }
	else
        {
	  value = atoi(argv[arg]);
	  printf("  L1_REPLACEMENT_POLICY:   %s\n",argv[arg]);
        }
        continue;
     }
*/

    if(arg==4)
    {
        printf("Victim_Cache_SIZE:     %s\n",argv[arg]);
        value = atoi(argv[arg]);
        if(is_power_of_2(value)==0 && value!=0)
        {
          printf("The cache size is not power of two\n");
          return false;
        }

        continue;
    }
/*
    if(arg==5) 
    {
	if(*argv[arg]==0)
        {
	  printf("  L1_WRITE_POLICY:    %s\n",argv[arg]);
        }
	else
        {
	  printf("  L1_WRITE_POLICY:    %s\n",argv[arg]);
        }
      continue;
    }
*/
     if(arg==5)
     {
        printf("  L2_SIZE:     %s\n",argv[arg]);
        value = atoi(argv[arg]);
        if(is_power_of_2(value)==0 && value!=0)
        {
          printf("The cache size is not power of two\n");
          return false;
        }
        continue;
     }

    if(arg==6)
    {
        printf("  L2_ASSOC:     %s\n",argv[arg]);
        value = atoi(argv[arg]);
        continue;
    }

  if(arg==7)
    {

        //printf("  Replacement Policy:   %s\n",argv[arg]);
          value = atof(argv[arg]);
	  temp=value;
  //      printf("value:%d\n",value);
         if(value!=2 && value!=3 && !(value>=0 && value<=1))
	{
          printf("  Your replacement policy input is not correct   \n");
          return false;
        }
        continue;
     }

  if(arg==8) 
    {
      printf("    trace_file:    %s\n",argv[arg]);
      if(temp==2)
      printf("  Replacement Policy:  LRU \n");
      if(temp==3)
      printf("  Replacement Policy:  LFU \n");
      if(temp>=0 && temp<=1)
{
      printf("  Replacement Policy:  LRFU \n"); 
      printf("  lambda: %.2f\n",temp);
}
      continue;
    }  
}
return true;
}

char* hexToBin(char* in){ 
    int x = 4;
    int size;
    size = strlen(in);
   //printf("size is %d\n",size);
   char input[]="00000000";
    int i;
    for (i = 0; i < size + 1; i++) 
    {
        input[8-size+i] = in[i];
    }

    //printf("buffer : %s\n", input);

    char* output = new char[8*4+1];

   
    for (int i = 0; i < 8; i++)
    {
        if (input[i] =='0') {
            output[i*x +0] = '0';
            output[i*x +1] = '0';
            output[i*x +2] = '0';
            output[i*x +3] = '0';
        }
        else if (input[i] =='1') {
            output[i*x +0] = '0';
            output[i*x +1] = '0';
            output[i*x +2] = '0';
            output[i*x +3] = '1';
        }    
        else if (input[i] =='2') {
            output[i*x +0] = '0';
            output[i*x +1] = '0';
            output[i*x +2] = '1';
            output[i*x +3] = '0';
        }    
        else if (input[i] =='3') {
            output[i*x +0] = '0';
            output[i*x +1] = '0';
            output[i*x +2] = '1';
            output[i*x +3] = '1';
        }    
        else if (input[i] =='4') {
            output[i*x +0] = '0';
            output[i*x +1] = '1';
            output[i*x +2] = '0';
            output[i*x +3] = '0';
        }    
        else if (input[i] =='5') {
            output[i*x +0] = '0';
            output[i*x +1] = '1';
            output[i*x +2] = '0';
            output[i*x +3] = '1';
        }    
        else if (input[i] =='6') {
            output[i*x +0] = '0';
            output[i*x +1] = '1';
            output[i*x +2] = '1';
            output[i*x +3] = '0';
        }    
        else if (input[i] =='7') {
            output[i*x +0] = '0';
            output[i*x +1] = '1';
            output[i*x +2] = '1';
            output[i*x +3] = '1';
        }    
        else if (input[i] =='8') {
            output[i*x +0] = '1';
            output[i*x +1] = '0';
            output[i*x +2] = '0';
            output[i*x +3] = '0';
        }
        else if (input[i] =='9') {
            output[i*x +0] = '1';
            output[i*x +1] = '0';
            output[i*x +2] = '0';
            output[i*x +3] = '1';
        }
        else if (input[i] =='a') {    
            output[i*x +0] = '1';
            output[i*x +1] = '0';
            output[i*x +2] = '1';
            output[i*x +3] = '0';
        }
        else if (input[i] =='b') {
            output[i*x +0] = '1';
            output[i*x +1] = '0';
            output[i*x +2] = '1';
            output[i*x +3] = '1';
        }
        else if (input[i] =='c') {
            output[i*x +0] = '1';
            output[i*x +1] = '1';
            output[i*x +2] = '0';
            output[i*x +3] = '0';
        }
        else if (input[i] =='d') {    
            output[i*x +0] = '1';
            output[i*x +1] = '1';
            output[i*x +2] = '0';
            output[i*x +3] = '1';
        }
        else if (input[i] =='e'){    
            output[i*x +0] = '1';
            output[i*x +1] = '1';
            output[i*x +2] = '1';
            output[i*x +3] = '0';
        }
        else if (input[i] =='f') {
            output[i*x +0] = '1';
            output[i*x +1] = '1';
            output[i*x +2] = '1';
            output[i*x +3] = '1';
        }
    }

    output[32] = '\0';
    //printf("strlen of output is %d\n",strlen(output));
    return output;
}

cache* L1_cache;
cache* L2_cache;
int config;
int access_count;



void fetch(inst_cache * inst)
{
ins_d *temp;
temp = fake_rob_end;

fake_rob_end = (ins_d *)malloc(sizeof(ins_d));

temp->next = fake_rob_end;

fake_rob_end->stage = IF;
fake_rob_end->pc = inst->ins_pc;
fake_rob_end->tag = inst->ins_tag;
fake_rob_end->type = inst->ins_type;
fake_rob_end->src1_ready = NOT_READY;
fake_rob_end->src2_ready = NOT_READY;
fake_rob_end->dest_value = -1;
fake_rob_end->src1_value = -1;
fake_rob_end->src2_value = -1;
fake_rob_end->dest_reg = inst->ins_dest_reg;
fake_rob_end->src1_reg = inst->ins_src1_reg;
fake_rob_end->src2_reg = inst->ins_src2_reg;
fake_rob_end->func_lat = inst->ins_func_lat;
fake_rob_end->next = fake_rob_head;

temp = dispatch_queue_end;

dispatch_queue_end = (ins_d *)malloc(sizeof(ins_d));
temp->next = dispatch_queue_end;

copy_queues_units(dispatch_queue_end,fake_rob_end);
dispatch_queue_end->next = dispatch_queue_head;

timing[inst->ins_tag].fetch_start_cycle = proc_cycles;
dispatch_count++;
}

void dispatch()
{
ins_d *temp_dispatch_prev,*temp_dispatch_next,*temp_rob,*temp_issue;
//int temp_dispatch_count = dispatch_count;
//int i = 0;

temp_dispatch_prev = dispatch_queue_head;
temp_dispatch_next = dispatch_queue_head->next;

while (temp_dispatch_next != dispatch_queue_head && issue_count < s) {
if (temp_dispatch_next->stage == ID)
{
   temp_issue = issue_queue_end;   
   issue_queue_end = (ins_d *)malloc(sizeof(ins_d));
   temp_issue->next = issue_queue_end;
   copy_queues_units(issue_queue_end, temp_dispatch_next);
   issue_queue_end->next = issue_queue_head;
   issue_count++;
   issue_queue_end->stage = IS;
   
   temp_dispatch_prev->next = temp_dispatch_next->next;
   dispatch_count--;

   if(temp_dispatch_next==dispatch_queue_end)
      dispatch_queue_end = temp_dispatch_prev;


temp_rob = fake_rob_head->next;
while (temp_rob != fake_rob_head) {
if (temp_rob->tag == issue_queue_end->tag) {
	temp_rob->stage = IS;
	break;
	}
	temp_rob = temp_rob->next;
}
timing[issue_queue_end->tag].dispatch_duration = proc_cycles - timing[issue_queue_end->tag].dispatch_start_cycle;
timing[issue_queue_end->tag].issue_start_cycle = proc_cycles;

if(issue_queue_end->src1_reg ==-1)
{
issue_queue_end->src1_ready = READY;
issue_queue_end->src1_value = -1;
temp_rob->src1_ready = READY;
}
else
{
if (r_file[issue_queue_end->src1_reg].ready == READY) {
  issue_queue_end->src1_ready = READY;
  issue_queue_end->src1_value = r_file[issue_queue_end->src1_reg].value;
  temp_rob->src1_ready = READY;  
} 
else if (r_file[issue_queue_end->src1_reg].ready == NOT_READY){
  issue_queue_end->src1_ready = NOT_READY;
  issue_queue_end->src1_value = r_file[issue_queue_end->src1_reg].value;
 }
}

if(issue_queue_end->src2_reg ==-1)
{
issue_queue_end->src2_ready = READY;
//issue_queue_end->src2_value = -1;
//temp_rob->src2_ready = READY;
}
else
{
if (r_file[issue_queue_end->src2_reg].ready == READY) {
  issue_queue_end->src2_ready = READY;
//  issue_queue_end->src2_value = r_file[issue_queue_end->src2_reg].value;
//  temp_rob->src2_ready = READY;
} 
else if (r_file[issue_queue_end->src2_reg].ready == NOT_READY){
  issue_queue_end->src2_ready = NOT_READY;
  issue_queue_end->src2_value = r_file[issue_queue_end->src2_reg].value;
 }
}

if (issue_queue_end->dest_reg == -1) {
//  issue_queue_end->dest_ready = READY;
  issue_queue_end->dest_value = -1;
}
else {
  r_file[issue_queue_end->dest_reg].ready = NOT_READY;
  r_file[issue_queue_end->dest_reg].value = issue_queue_end->tag;
}
}
temp_dispatch_next = temp_dispatch_next->next;
}

temp_dispatch_next = dispatch_queue_head->next;
while(temp_dispatch_next!=dispatch_queue_head)
{
if(temp_dispatch_next->stage == IF)
{
temp_dispatch_next->stage = ID;
temp_rob = fake_rob_head->next;
while (temp_rob != fake_rob_head) {
if (temp_rob->tag == temp_dispatch_next->tag) {
  temp_rob->stage = ID;
  break;
  }
  temp_rob = temp_rob->next;
}
timing[temp_dispatch_next->tag].fetch_duration = proc_cycles - timing[temp_dispatch_next->tag].fetch_start_cycle;
timing[temp_dispatch_next->tag].dispatch_start_cycle = proc_cycles;
temp_dispatch_prev = temp_dispatch_next;
}
temp_dispatch_next = temp_dispatch_next->next;
}
} 

void issue()
{
char* return_addr_L1=NULL;
char* return_addr_L2=NULL;
//char temp[8];
ins_d *temp_issue_prev,*temp_issue_next,*temp_rob,*temp_execute;
//int temp_issue_count = issue_count;
//int i = 0;

temp_issue_prev = issue_queue_head;
temp_issue_next = issue_queue_head->next;
execute_count=0;
while (temp_issue_next !=issue_queue_head && execute_count < n) {
if (temp_issue_next->stage == IS && temp_issue_next->src1_ready==READY && temp_issue_next->src2_ready==READY)
{ 
   temp_execute = execute_queue_end;   
   execute_queue_end = (ins_d *)malloc(sizeof(ins_d));
   temp_execute->next = execute_queue_end;
   copy_queues_units(execute_queue_end, temp_issue_next);
   execute_queue_end->next = execute_queue_head;
   execute_count++;
   execute_queue_end->stage = EX;
  
if(ins_cache[execute_queue_end->tag].ins_mem_addr[0]!='0')
{
access_count++;

if(config==0)
{
//strcpy(temp,ins_cache[execute_queue_end->tag].ins_mem_addr);
return_addr_L1 = L1_cache->read(hexToBin(&ins_cache[execute_queue_end->tag].ins_mem_addr[0]));
 if(return_addr_L1!=NULL)
  {
  ins_cache[execute_queue_end->tag].ins_func_lat = 20;
  ins_cache[execute_queue_end->tag].ins_type =  4;
  execute_queue_end->type = 4;  
  execute_queue_end->func_lat = 20;
  }
}
else if(config==1)
{
return_addr_L1 = L1_cache->read(hexToBin(&ins_cache[execute_queue_end->tag].ins_mem_addr[0]));
if(return_addr_L1!=NULL)
{
 if(strcmp(return_addr_L1,(hexToBin(&ins_cache[execute_queue_end->tag].ins_mem_addr[0])))==0)
 {
  //printf("Entered\n");
  return_addr_L2=L2_cache->read(hexToBin(&ins_cache[execute_queue_end->tag].ins_mem_addr[0]));
 if(return_addr_L2!=NULL)
 {
   ins_cache[execute_queue_end->tag].ins_func_lat = 20;
   ins_cache[execute_queue_end->tag].ins_type =  4;
   execute_queue_end->type = 4;
   execute_queue_end->func_lat = 20;
 }
 else
 {
   ins_cache[execute_queue_end->tag].ins_func_lat = 10;
   ins_cache[execute_queue_end->tag].ins_type =  3;
   execute_queue_end->type = 3;
   execute_queue_end->func_lat = 10;
 }     
 }
 else
 {
 // printf("I am address%s\n",return_addr_L1);
 return_addr_L2=L2_cache->write(return_addr_L1);
 return_addr_L2=L2_cache->read(hexToBin(&ins_cache[execute_queue_end->tag].ins_mem_addr[0]));
 }  
}
}
}

 
   temp_issue_prev->next = temp_issue_next->next;
   issue_count--;

   if(temp_issue_next==issue_queue_end)
      issue_queue_end = temp_issue_prev;

temp_rob = fake_rob_head->next;
while (temp_rob != fake_rob_head) {
if (temp_rob->tag == execute_queue_end->tag) {
	temp_rob->stage = EX;
        temp_rob->func_lat = execute_queue_end->func_lat;
	break;
	}
	temp_rob = temp_rob->next;
}
timing[execute_queue_end->tag].issue_duration = proc_cycles - timing[execute_queue_end->tag].issue_start_cycle;
timing[execute_queue_end->tag].execute_start_cycle = proc_cycles;
}

else
temp_issue_prev = temp_issue_next;

temp_issue_next = temp_issue_next->next;
}
}

void execute()
{
ins_d *temp_execute_prev,*temp_execute_next,*temp_rob,*temp_issue;
//int temp_execute_count = execute_count;
//int i = 0;

temp_execute_prev = execute_queue_head;
temp_execute_next = execute_queue_head->next;
//temp_rob_end = fake_rob_end;

while(temp_execute_next!=execute_queue_head)
{
temp_execute_next->func_lat--;
temp_rob = fake_rob_head->next;
while (temp_rob!=fake_rob_head) {
if (temp_rob->tag == temp_execute_next->tag){
  temp_rob->func_lat--;
  break;
  }
  temp_rob = temp_rob->next;
}
if(temp_execute_next->func_lat==0)
{
temp_rob->stage = WB;
if(temp_execute_next->type==0)
 timing[temp_execute_next->tag].execute_duration =1;
else if(temp_execute_next->type==1)
 timing[temp_execute_next->tag].execute_duration = 2;
else if(temp_execute_next->type==2)
 timing[temp_execute_next->tag].execute_duration =5;
else if(temp_execute_next->type==3)
 timing[temp_execute_next->tag].execute_duration =10;
else if(temp_execute_next->type==4)
 timing[temp_execute_next->tag].execute_duration =20;

timing[temp_execute_next->tag].writeback_start_cycle = proc_cycles;
temp_execute_prev->next = temp_execute_next->next;
execute_count--;

   if(temp_execute_next==execute_queue_end)
      execute_queue_end = temp_execute_prev;

if (r_file[temp_execute_next->dest_reg].ready == NOT_READY && r_file[temp_execute_next->dest_reg].value == temp_execute_next->tag) 
	r_file[temp_execute_next->dest_reg].ready = READY;

temp_issue = issue_queue_head->next;
while(temp_issue!=issue_queue_head){
if (temp_issue->src1_ready == NOT_READY && temp_issue->src1_value == temp_execute_next->tag)
    temp_issue->src1_ready = READY;
else if (temp_issue->src2_ready == NOT_READY && temp_issue->src2_value == temp_execute_next->tag)
    temp_issue->src2_ready = READY;
temp_issue = temp_issue->next;
}
}
else
temp_execute_prev = temp_execute_next;

temp_execute_next = temp_execute_next->next;
}
}

void fake_retire()
{
ins_d *temp_fake_prev, *temp_fake_next;
temp_fake_prev = fake_rob_head;
temp_fake_next = fake_rob_head->next;

while(temp_fake_next!=fake_rob_head){
if(temp_fake_next->stage == WB)
timing[temp_fake_next->tag].writeback_duration = 1;
temp_fake_next = temp_fake_next->next;
}
temp_fake_next = fake_rob_head->next;
while(temp_fake_next!=fake_rob_head){
if(temp_fake_next->stage == WB)
{
//timing[temp_fake_next->tag].writeback_duration = proc_cycles-timing[temp_fake_next->tag].writeback_start_cycle;
temp_fake_prev->next = temp_fake_next->next;

   if(temp_fake_next==fake_rob_end)
      fake_rob_end = temp_fake_prev;

printf("%d fu{%d} src{%d,%d} dst{%d} IF{%d,%d} ID{%d,%d} IS{%d,%d} EX{%d,%d} WB{%d,%d}\n",temp_fake_next->tag, temp_fake_next->type, temp_fake_next->src1_reg, temp_fake_next->src2_reg, temp_fake_next->dest_reg,timing[temp_fake_next->tag].fetch_start_cycle, timing[temp_fake_next->tag].fetch_duration,timing[temp_fake_next->tag].dispatch_start_cycle, timing[temp_fake_next->tag].dispatch_duration,timing[temp_fake_next->tag].issue_start_cycle, timing[temp_fake_next->tag].issue_duration,timing[temp_fake_next->tag].execute_start_cycle, timing[temp_fake_next->tag].execute_duration,timing[temp_fake_next->tag].writeback_start_cycle, timing[temp_fake_next->tag].writeback_duration);
}
else
break;
temp_fake_next = temp_fake_next->next;
}
}



int main(int argc, char *argv[])
{


if (argc != 9) {
	printf("PLEASE CHECK THE ARGUMENTS YOU HAVE GIVEN\n");
	exit(-1);
	}
L1_cache=new cache();
L2_cache=new cache();
config=-1;
access_count=0;

s = atoi(argv[1]);
n = atoi(argv[2]);
bs = atoi(argv[3]);
L1_cache->set_cache_param(1, bs);
L2_cache->set_cache_param(1, bs);
lps = atoi(argv[4]);
L1_cache->set_cache_param(2, lps);
lpa = atoi(argv[5]);
L1_cache->set_cache_param(3, lpa);
lss = atoi(argv[6]);
L2_cache->set_cache_param(2, lss);
lsa =  atoi(argv[7]);
L2_cache->set_cache_param(3, lsa);

L1_cache->set_cache_param(4,0);
L2_cache->set_cache_param(4,0);

L1_cache->set_cache_param(5, 0);
L2_cache->set_cache_param(5, 0);
L2_cache->set_cache_param(6, 0);

if(lps!=0)
{
L1_cache->init_cache();
config=0;
}

if(lss!=0)
{
L2_cache->init_cache();
config = 1;
}
	
FILE* inFile;
char * fileString = argv[argc-1];

inFile = freopen(fileString,"r",stdin);

   if(inFile==NULL)
   {
    printf("PLEASE CHECK YOUR FILE \n \n");
   }

int count = 0;

fake_rob_head = (ins_d *)malloc(sizeof(ins_d));
dispatch_queue_head = (ins_d *)malloc(sizeof(ins_d));
issue_queue_head = (ins_d *)malloc(sizeof(ins_d));
execute_queue_head = (ins_d *)malloc(sizeof(ins_d));



*fake_rob_head = ins_d_init;
*dispatch_queue_head = ins_d_init;
*issue_queue_head = ins_d_init;
*execute_queue_head = ins_d_init;

fake_rob_end = fake_rob_head;
dispatch_queue_end=dispatch_queue_head;
issue_queue_end=issue_queue_head;
execute_queue_end=execute_queue_head;

fake_rob_head->next = fake_rob_head;
dispatch_queue_head->next = dispatch_queue_head;
issue_queue_head->next = issue_queue_head;
execute_queue_head->next = execute_queue_head;


for (int i=0 ; i < 128; i++) {
  r_file[i].ready = READY;
  r_file[i].value = i;
}
//char temp[8];
while(true)
{

fscanf(stdin,"%x",&ins_cache[count].ins_pc);
fscanf(stdin,"%d",&ins_cache[count].ins_type);
fscanf(stdin,"%d",&ins_cache[count].ins_dest_reg);
fscanf(stdin,"%d",&ins_cache[count].ins_src1_reg);
fscanf(stdin,"%d",&ins_cache[count].ins_src2_reg);
fscanf(stdin,"%s",ins_cache[count].ins_mem_addr);

ins_cache[count].ins_mem_addr[9]='\0';
ins_cache[count].ins_tag = count;

if(ins_cache[count].ins_type ==  0)
ins_cache[count].ins_func_lat = 1;

if(ins_cache[count].ins_type ==  1)
ins_cache[count].ins_func_lat = 2;

if(ins_cache[count].ins_type ==  2)
ins_cache[count].ins_func_lat = 5;

if(feof(stdin)) break;
count++;
}

fclose(stdin);

timing = (timing_stage *)malloc(sizeof(timing_stage) * count);
//exit(1);

//for (i=0 ; i < count ; i++) {
//printf("%x %d %d %d %d %x\n", ins_cache[i].ins_pc, ins_cache[i].ins_type, ins_cache[i].ins_dest_reg, ins_cache[i].ins_src1_reg, ins_cache[i].ins_src2_reg, ins_cache[i].ins_mem_addr);
//}
int pc_iteration=0;
int fetch_count = 0;
ins_d *temp_print;

while((pc_iteration < count && fake_rob_head!=fake_rob_end) || (pc_iteration < count && fake_rob_head==fake_rob_end) || (pc_iteration >= count && fake_rob_head!=fake_rob_end))
{
fake_retire();
/*
temp_print = fake_rob_head->next;
printf("Fake rob starts\n");
while(temp_print!= fake_rob_head)
{
printf("%d %x %d %d %d %d %d %d %d %d %d %d %d %p\n\n",temp_print->stage,temp_print->pc,temp_print->tag,temp_print->type,temp_print->src1_ready,temp_print->src2_ready,temp_print->dest_value,temp_print->src1_value,temp_print->src2_value,temp_print->dest_reg,temp_print->src1_reg,temp_print->src2_reg,temp_print->func_lat,temp_print->next);
temp_print=temp_print->next;
}
*/
execute();
/*
temp_print = execute_queue_head->next;
printf("Execute queue starts\n");
while(temp_print!= execute_queue_head)
{
printf("%d %x %d %d %d %d %d %d %d %d %d %d %d %p\n\n",temp_print->stage,temp_print->pc,temp_print->tag,temp_print->type,temp_print->src1_ready,temp_print->src2_ready,temp_print->dest_value,temp_print->src1_value,temp_print->src2_value,temp_print->dest_reg,temp_print->src1_reg,temp_print->src2_reg,temp_print->func_lat,temp_print->next);
temp_print= temp_print->next;
}
*/

issue();
/*
temp_print = issue_queue_head->next;
printf("Issue queue starts\n");
while(temp_print!= issue_queue_head)
{
printf("%d %x %d %d %d %d %d %d %d %d %d %d %d %p\n\n",temp_print->stage,temp_print->pc,temp_print->tag,temp_print->type,temp_print->src1_ready,temp_print->src2_ready,temp_print->dest_value,temp_print->src1_value,temp_print->src2_value,temp_print->dest_reg,temp_print->src1_reg,temp_print->src2_reg,temp_print->func_lat,temp_print->next);
temp_print=temp_print->next;
}
*/
dispatch();
/*
temp_print = dispatch_queue_head->next;
printf("Dispatch queue starts\n");
while(temp_print!= dispatch_queue_head)
{
printf("%d %x %d %d %d %d %d %d %d %d %d %d %d %p\n\n",temp_print->stage,temp_print->pc,temp_print->tag,temp_print->type,temp_print->src1_ready,temp_print->src2_ready,temp_print->dest_value,temp_print->src1_value,temp_print->src2_value,temp_print->dest_reg,temp_print->src1_reg,temp_print->src2_reg,temp_print->func_lat,temp_print->next);
temp_print=temp_print->next;
}
*/
fetch_count=0;
while (pc_iteration < count &&  fetch_count< n && dispatch_count< 2*n) {
fetch(&ins_cache[pc_iteration]);
pc_iteration++;
fetch_count++;
}
proc_cycles++;
}
proc_cycles--;

if(lps!=0)
{
printf("L1 CACHE CONTENTS\n");
printf("a. number of accesses :%d\n",L1_cache->access_count);
printf("b. number of misses :%d\n",L1_cache->read_miss_count);
for(int i=0;i<L1_cache->num_sets;i++)
{
printf("set%d:",i);
for(int j=0;j<L1_cache->set_associativity;j++)
{
if(L1_cache->sets[i]->blocks[j]->dirty)
printf("  %x   D   ",bintohex(L1_cache->sets[i]->blocks[j]->tag));
else
printf("  %x       ",bintohex(L1_cache->sets[i]->blocks[j]->tag));
}
printf("\n");
}
}

if(lss!=0)
{
printf("\n");
printf("L2 CACHE CONTENTS\n");
printf("a. number of accesses :%d\n",L2_cache->access_count);
printf("b. number of misses :%d\n",L2_cache->read_miss_count);
for(int i=0;i<L2_cache->num_sets;i++)
{
printf("set%d:",i);
for(int j=0;j<L2_cache->set_associativity;j++)
{
if(L2_cache->sets[i]->blocks[j]->dirty)
printf("  %x   D   ",bintohex(L2_cache->sets[i]->blocks[j]->tag));
else
printf("  %x       ",bintohex(L2_cache->sets[i]->blocks[j]->tag));
}
printf("\n");
}
}
//printf("\n");
printf("CONFIGURATION\n");
printf(" superscalar bandwidth (N) = %d\n", n);
printf(" dispatch queue size (2*N) = %d\n", 2*n);
printf(" schedule queue size (S)   = %d\n", s);
printf("RESULTS\n");
printf(" number of instructions = %d\n", count);
printf(" number of cycles       = %d\n", proc_cycles);
float ipc = ((float)count)/((float)proc_cycles);
printf(" IPC                    = %.2f\n", ipc);


}




