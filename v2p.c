#include <types.h>
#include <mmap.h>
#include <fork.h>
#include <v2p.h>
#include <page.h>

/*
 * You may define macros and other helper functions here

 * You must not declare and use any static/global variables
 * */
#define OS_PT_REG   1
#define USER_REG    2

#define PAGE_PRESENT 0x001
#define PAGE_RW      0x002
#define PAGE_USER    0x004

#define PAGE_FRAME_MASK 0xFFFFFFFFFFFFF000ULL
#define PFN_MASK 0x000FFFFFFFFFF000ULL

long mmap_helper(struct vm_area *vm_area, u64 addr, int length, int prot);
void mprotect_helper(struct exec_context *current, u64 addr, int prot) ;

u64* extract(u64 pgd,u64 addr)
{
    u64 MASK = 0x1FF ;
    u64 * pgd_t =  (u64 *)osmap(pgd) ;
    u64 pud = (pgd_t[((addr & (MASK << 39)) >>  39)] >> 12) ;
    u64 * pud_t = (u64 *)osmap(pud) ;
    u64 pmd = (pud_t[((addr & (MASK << 30)) >>  30)] >> 12) ;
    u64 * pmd_t = (u64 *)osmap(pmd) ;
    u64 pte = (pmd_t[((addr & (MASK << 21)) >>  21)] >> 12) ;
    u64 * pte_t = (u64 *)osmap(pte) ;

    return &pte_t[((addr & (MASK << 12)) >>  12)] ;
}

void setzero(void* ptr, int size){
    char* ptr_mine = (char*) ptr;
    for(int i=0; i<size; i++){
        *(ptr_mine + i) = 0;
    }
}

u64 phy_addr(u64 pgd_vir_base, unsigned long cur_vir_adr){
    
    u64* pgd_vir = (u64*)pgd_vir_base;
    u64 pgd_entry, pud_entry, pmd_entry, pte_entry;
    u64 *pud_vir, *pmd_vir, *pte_vir;
    u64 offset    = cur_vir_adr & 0xFFF;

    u64 pgd_index = (cur_vir_adr >> 39) & 0x1FF;
    u64 pud_index = (cur_vir_adr >> 30) & 0x1FF;
    u64 pmd_index = (cur_vir_adr >> 21) & 0x1FF;
    u64 pte_index = (cur_vir_adr >> 12) & 0x1FF;

    pgd_entry = pgd_vir[pgd_index];
    if (!(pgd_entry & PAGE_PRESENT))
        return 0;

    pud_vir = (u64*)osmap((pgd_entry & PFN_MASK) >> 12);
    pud_entry = pud_vir[pud_index];
    if (!(pud_entry & PAGE_PRESENT)) {
        return 0;
    }

    pmd_vir = (u64*)osmap((pud_entry & PFN_MASK) >> 12);
    pmd_entry = pmd_vir[pmd_index];
    if (!(pmd_entry & PAGE_PRESENT)) {
        return 0;
    }

    pte_vir = (u64*)osmap((pmd_entry & PFN_MASK) >> 12);
    pte_entry = pte_vir[pte_index];
    if (!(pte_entry & PAGE_PRESENT)) {
        return 0;
    }

    u64 frame_base = pte_entry & PFN_MASK;
    return frame_base + offset;
}

int create_page(u64* pgd_vir_base, unsigned long cur_vir_adr, u32 access_flags) {
    u64* pgd_vir = (u64*)pgd_vir_base;
    u64 pgd_entry, pud_entry, pmd_entry, pte_entry;
    u64 *pud_vir, *pmd_vir, *pte_vir;

    u64 pgd_index = (cur_vir_adr >> 39) & 0x1FF;
    u64 pud_index = (cur_vir_adr >> 30) & 0x1FF;
    u64 pmd_index = (cur_vir_adr >> 21) & 0x1FF;
    u64 pte_index = (cur_vir_adr >> 12) & 0x1FF;

    pgd_entry = pgd_vir[pgd_index];
    if (!(pgd_entry & PAGE_PRESENT))
    {
        u64 new_pud_phy = os_pfn_alloc(OS_PT_REG);
        if (new_pud_phy == NULL){
            return -1;
        }
        get_pfn(new_pud_phy);
        u64 new_pud_vir = (u64)osmap(new_pud_phy);
        setzero((void*)new_pud_vir, 4096);
        pud_vir[pud_index] = (new_pud_phy << 12) | PAGE_PRESENT | PAGE_RW | PAGE_USER;
        pud_entry = pud_vir[pud_index];
    }
        

    pud_vir = (u64*)osmap((pgd_entry & PFN_MASK) >> 12);
    pud_entry = pud_vir[pud_index];
    if (!(pud_entry & PAGE_PRESENT)) {
        u64 new_pmd_phy = os_pfn_alloc(OS_PT_REG);
        if (new_pmd_phy == NULL){
            return -1;
        }
        get_pfn(new_pmd_phy);
        u64 new_pmd_vir = (u64)osmap(new_pmd_phy);
        setzero((void*)new_pmd_vir, 4096);
        pud_vir[pud_index] = (new_pmd_phy << 12) | PAGE_PRESENT | PAGE_RW | PAGE_USER;
        pud_entry = pud_vir[pud_index];
    } 
    
    pmd_vir = (u64*)osmap((pud_entry & PFN_MASK) >> 12);
    pmd_entry = pmd_vir[pmd_index];
    if (!(pmd_entry & PAGE_PRESENT)) {
        u64 new_pte_phy = os_pfn_alloc(OS_PT_REG);
        if (new_pte_phy == 0) {
            return -1;
        }
        get_pfn(new_pte_phy);
        u64 new_pte_vir = (u64)osmap(new_pte_phy);
        setzero((void*)new_pte_vir, 4096);
        pmd_vir[pmd_index] = (new_pte_phy << 12) | PAGE_PRESENT | PAGE_RW | PAGE_USER;
        pmd_entry = pmd_vir[pmd_index];
    } 
    
    pte_vir = (u64*)osmap((pmd_entry & PFN_MASK) >> 12);
    pte_entry = pte_vir[pte_index];
    if (!(pte_entry & PAGE_PRESENT)) {
        u64 new_page_phy = os_pfn_alloc(USER_REG);
        if (new_page_phy == 0) {
            return -1;
        }
        get_pfn(new_page_phy);
        u64 new_page_vir = (u64)osmap(new_page_phy);
        setzero((void*)new_page_vir, 4096);
        pte_vir[pte_index] = (new_page_phy << 12) | PAGE_PRESENT | access_flags | PAGE_USER;
    } 
    
    return 1;
}

void find_node(struct vm_area* prev, struct vm_area* now, struct vm_area* next, u64 addr){
   int d = 0;
   while(d == 0){
        u64 start_now  = now->vm_start;
        u64 end_now    = now->vm_end;
        u64 start_next = next->vm_start;
        u64 end_next   = next->vm_end;

     if(end_now <= addr)
     {
             prev = now;
             now  = next;
             next = next->vm_next;    
     } else d = 1 ;
     
    } 
}

/**
 * mprotect System call Implementation.
 */
long vm_area_mprotect(struct exec_context *current, u64 addr, int length, int prot)
{
    struct vm_area *dummy = current -> vm_area ; 
    if(length % 4096) length += 4096 - (length % 4096) ; // making length multiple of 4KB

    if(dummy == NULL) return 0 ; // if no VMA's , job is done
    
    struct vm_area *prev = dummy ; // previous pointer
    while(dummy -> vm_next != NULL && dummy -> vm_end <= addr)
    {
        if(prev != dummy) prev = prev -> vm_next ;
        dummy = dummy -> vm_next ;

        if(dummy -> vm_next == NULL && dummy -> vm_end < addr) return 0 ; // VMA region end <= addr
    }

    while(dummy != NULL && dummy -> vm_start < addr + length)
    {
        if(prot == dummy -> access_flags) {dummy = dummy -> vm_next ; prev = prev -> vm_next ; continue ;}
        // no protection change 

        if(dummy -> vm_start < addr) // left portion of node outside mprotect 
        {
               struct vm_area *NEW = os_alloc(sizeof(struct vm_area)) ; // creating new node
               stats -> num_vm_area ++ ; // splitting

               NEW -> vm_next = dummy -> vm_next ;
               dummy -> vm_next = NEW ;
               NEW -> vm_end = dummy -> vm_end ;

               dummy -> vm_end = NEW -> vm_start = addr ;
               NEW -> access_flags = prot ; prev = dummy ; dummy = NEW ; 

               if(dummy -> vm_end > addr + length)
               {
                  NEW = os_alloc(sizeof(struct vm_area)) ; // creating new node
                  stats -> num_vm_area ++ ; // splitting
                  NEW -> access_flags = prev -> access_flags ;

                  NEW -> vm_end = dummy -> vm_end ; dummy -> vm_next = NEW ;
                  NEW -> vm_start = dummy -> vm_end = addr + length ;
               }
        }
        else if(dummy -> vm_end > addr + length) // right portion of node outside mprotect
        {
            if(prot == prev -> access_flags && prev -> vm_end == dummy -> vm_start)
            {
                 prev -> vm_end = addr + length ;
                 dummy -> vm_start = addr + length ;
            }
            else
            {
               struct vm_area *NEW = os_alloc(sizeof(struct vm_area)) ; // creating new node
               stats -> num_vm_area ++ ; // splitting

               prev -> vm_next = NEW ;  prev = NEW ;
               NEW -> vm_next = dummy ;

               NEW -> access_flags = prot ;
               NEW -> vm_start = dummy -> vm_start ;
               NEW -> vm_end = dummy -> vm_start = addr + length ;

            }   
        }
        else 
        {
            if(prot == prev -> access_flags && prev -> vm_end == dummy -> vm_start)
            {
                 prev -> vm_end = dummy -> vm_end ;
                 prev -> vm_next = dummy -> vm_next ; 
                 os_free(dummy,sizeof(struct vm_area)) ;
                 stats -> num_vm_area -- ; // merging
                 dummy = prev ; // current node becomes previous
            }
            else   dummy -> access_flags = prot ;
        }

        prev = dummy ;
        dummy = dummy -> vm_next ;
    }

    if(dummy != NULL && prev -> vm_end == dummy -> vm_start && prev -> access_flags == dummy -> access_flags)
    {
        prev -> vm_end = dummy -> vm_end ;
        prev -> vm_next = dummy -> vm_next ;
        os_free(dummy,sizeof(struct vm_area)) ;
        stats -> num_vm_area -- ; // merging
    }
    
    for(u64 i = 0 ; i < length/4096 ; i++)
    {
        mprotect_helper(current, addr + i*4096, prot) ; // physical implementation
    }

    return 0 ;


    return -EINVAL;
}

void mprotect_helper(struct exec_context *current, u64 addr, int prot) 
{
   u64 MASK = 0xFFF ;
   u64* pfn =  extract(current -> pgd,addr) ;
   
   if((*pfn) % 2 == 0) return ; 
   else 
   {
        s8 ct =  get_pfn_refcount(*pfn) ;
        if(ct > 1)
        {
            u32 new_pfn = os_pfn_alloc(USER_REG) ;
            *pfn &= 0xFFF0000000000FFF ;  
            *pfn |= (new_pfn << 12) ;
        }

        (*pfn) |= 8 ; // 4th bit
        if(prot == PROT_READ) *pfn -= 8 ;
   }
   
}


/**
 * mmap system call implementation.
 */

long mmap_helper(struct vm_area *dummy, u64 addr, int length, int prot)
{
    struct vm_area *prev = dummy;   if(length % 4096) length += 4096 - (length % 4096) ; // making length multiple of 4KB
    struct vm_area *curr = (struct vm_area *)(os_alloc(sizeof(struct vm_area))); // creating new node
    stats -> num_vm_area ++ ;

    if (addr == NULL)
    {
        while (dummy->vm_next != NULL && (dummy->vm_start - prev->vm_end <= length || dummy == prev)) 
        {
            if(dummy != prev) prev = prev -> vm_next ; 
            dummy = dummy -> vm_next ;
        } 
            if(dummy == prev)
            {
                
                if(length + dummy->vm_end > MMAP_AREA_END)
                {
                    os_free(curr, sizeof(struct vm_area)); stats -> num_vm_area -- ;
                    return -EINVAL;
                }
                else
                {
                    curr -> vm_start = dummy -> vm_end ;
                    curr -> access_flags = prot ;
                    curr -> vm_end = curr -> vm_start + length ;
                    dummy -> vm_next = curr ;
                    return curr -> vm_start ;
                }
            }
            else if (dummy->vm_next == NULL && dummy->vm_start - prev->vm_end < length)
            {
    
                if (dummy->vm_end + length > MMAP_AREA_END)
                {
                    os_free(curr, sizeof(struct vm_area)); stats -> num_vm_area -- ;
                    return -EINVAL;
                }

                else if (dummy->access_flags == prot)
                {
                    dummy->vm_end += length; 
                    os_free(curr, sizeof(struct vm_area)); stats -> num_vm_area -- ;
                    return dummy->vm_end - length;
                }
                else
                {  
                    curr->vm_start = dummy->vm_end; 
                    curr->vm_end = curr->vm_start + length;
                    curr->access_flags = prot;
                    dummy->vm_next = curr;
                    curr->vm_next = NULL; 

                    return curr->vm_start;
                }
            }
            else
            {
                if (dummy->vm_start - prev->vm_end == length &&
                    dummy->access_flags == prev->access_flags && prev->access_flags == prot)
                {
                    long temp = prev->vm_end;
                    prev->vm_end = dummy->vm_end;

                    os_free(curr, sizeof(struct vm_area)); stats -> num_vm_area -- ;
                    os_free(dummy, sizeof(struct vm_area)); stats -> num_vm_area -- ;
                    prev->vm_next = dummy->vm_next;

                    return temp;
                }
                else if (prev->access_flags == prot)
                {
                    prev->vm_end += length;
                    os_free(curr, sizeof(struct vm_area)); stats -> num_vm_area -- ;

                    return prev->vm_end - length;
                }
                else if (dummy->access_flags == prot)
                {
                    dummy->vm_start -= length;
                    os_free(curr, sizeof(struct vm_area)); stats -> num_vm_area -- ;

                    return dummy->vm_start;
                }
                else
                {
                    curr->vm_start = prev->vm_end;
                    prev->vm_next = curr;
                    curr->vm_next = dummy;
                    curr->vm_end = curr->vm_start + length;

                    return curr->vm_start;
                }
            }
    }
    curr->vm_start = addr;
    curr->vm_end = addr + length;
    curr->access_flags = prot;

    while (dummy->vm_next != NULL && dummy->vm_end <= addr)
    {
        if(dummy != prev) prev = dummy;
        dummy = dummy->vm_next;
    }

    if (dummy->vm_next == NULL && dummy->vm_end <= addr)
    {
        if (addr + length <= MMAP_AREA_END) // adding at end of list
        {
            if (dummy->vm_end == addr && prot == dummy->access_flags)
            {
                dummy->vm_end += length;
                os_free(curr, sizeof(struct vm_area)); stats -> num_vm_area -- ;
            }

            else
                dummy->vm_next = curr;
        }
        else 
        {
            os_free(curr, sizeof(struct vm_area)); stats -> num_vm_area -- ;
            return -EINVAL ;
        }

        return addr;
    }

    if (prev->vm_end > addr || addr + length > dummy->vm_start) // checking for overlap
    {
        os_free(curr, sizeof(struct vm_area)); stats -> num_vm_area -- ;
        return -EINVAL; // violating boundaries
    }
    int res = 0; // checker for merging

    if (prev->vm_end == addr && prev->access_flags == prot)
    {
        prev->vm_end += length;
        res += 2;
    }

    if (dummy->vm_start == addr + length && dummy->access_flags == prot)
    {
        dummy->vm_start -= length;
        res++;
    }

    if (res == 3) // merging both sides
    {
        prev->vm_next = dummy->vm_next;
        prev->vm_end = dummy->vm_end;
        os_free(dummy, sizeof(struct vm_area)); stats -> num_vm_area -- ;
        os_free(curr, sizeof(struct vm_area)); stats -> num_vm_area -- ;
    }

    else if (res)
    {  os_free(curr, sizeof(struct vm_area)); // merging one side
       stats -> num_vm_area -- ;
    }

    else // adding in between
    {
        curr->vm_next = prev->vm_next;
        prev->vm_next = curr;
    }
    return addr; // returning start of new vm_area
}

long vm_area_map(struct exec_context *current, u64 addr, int length, int prot, int flags)
{
    struct vm_area *dummy = current->vm_area; 
    

    if (dummy == NULL) // checking if first node is dummy
    {
        current->vm_area = dummy = (struct vm_area *)(os_alloc(sizeof(struct vm_area))); // allocating memory if not allocated

        /* INITIALISATION */
        dummy->vm_start = MMAP_AREA_START; 
        dummy->vm_end = MMAP_AREA_START + 4096;
        dummy->access_flags = 0x0;
        dummy->vm_next = NULL; 

        stats -> num_vm_area ++ ; // incrementing counter
    } 

    if (flags == MAP_FIXED)
    {
        if (addr == NULL || addr + length > MMAP_AREA_END) return -EINVAL;
            
        return mmap_helper(dummy, addr, length, prot);
    }

    else
    {
        long res;
        if ((res = mmap_helper(dummy, addr, length, prot)) != -EINVAL) return res;
            
        return mmap_helper(dummy, NULL, length, prot);
    }

    return -EINVAL;
}

/**
 * munmap system call implemenations
 */

long vm_area_unmap(struct exec_context *current, u64 addr, int length)
{
    struct vm_area* now = current -> vm_area;
    // if (addr > now->vm_start && addr > now->vm_end){
       //     now = now->vm_next;
       // }
    int length_mine = length;
    if(length % 4096 != 0){
        length_mine = 4096 * ((int)(length_mine/4096) + 1);
       }
       
   struct vm_area* next = now -> vm_next;
   struct vm_area* prev = os_alloc(sizeof(struct vm_area));

   u64 max = addr + (u64)length_mine;

    // Finding appropriate node in vm_area linked list
   find_node(prev, now, next,addr); 
   

   if(addr > now->vm_start && max == now->vm_end){
      //  prev->vm_next = now->vm_next;
      now -> vm_end = addr;
      //  os_free(now, sizeof(struct vm_area));
       return 0;
   }
   else if(max < now->vm_end && addr == now->vm_start){
      //  prev->vm_next = now->vm_next;
      now -> vm_start = max;
      //  os_free(now, sizeof(struct vm_area));
       return 0;
   }
   else if(addr > now->vm_start && max < now->vm_end){
       struct vm_area* new = os_alloc(sizeof(struct vm_area));
       
       new->vm_end = now->vm_end;
       now->vm_end = addr;
       new->vm_start = max;
       new->vm_next = now->vm_next;
       now->vm_next = new;
      //  os_free((void*)addr, sizeof(struct vm_area));
       return 0;
   }
   else if(addr > now->vm_start && max > now->vm_end){
       struct vm_area* next_temp = next;
       u64 traversed  = 0;
       
       if(next_temp == NULL){
          now->vm_next = next_temp;
          now->vm_end  = addr;
       }
       else{
           int check = 1; 
           while(traversed < (u64)length_mine){
              if(max <= next_temp->vm_end && max >= next_temp->vm_start){
                   traversed =  (u64)length_mine; 
                   now -> vm_end = addr;
                   next_temp -> vm_start = max; 
                   if(max == next_temp->vm_end){
                       now -> vm_next = next_temp->vm_next;
                       check = 0;
                       os_free(next_temp, sizeof(struct vm_area));
                      }
                  }
                  
                  else if(max < next_temp->vm_end && max < next_temp->vm_start){
                      traversed =  (u64)length_mine; 
                      now -> vm_end = addr;
                  }
                  
                  else{
                      struct vm_area* dummy = next_temp->vm_next;
                      
                      if(dummy->vm_start <= max){
                          traversed = (dummy)->vm_start - addr;
                          os_free(next_temp, sizeof(struct vm_area));
                          next_temp = dummy;
                      }
                      else{
                          traversed =  (u64)length_mine; 
                          check = 0;
                          os_free(next_temp, sizeof(struct vm_area));
                      }
                  }
           }
           if(check == 1){
              now->vm_next = next_temp;
          }
       }
       
      //  now->vm_next = NULL;
      //  now->vm_next = next_temp;
      //  os_free((void*)addr, sizeof(struct vm_area));
      return 0;
   }
   else if(addr < now->vm_start && max < now->vm_end){
       now->vm_start = max;
      //  os_free((void*)addr, sizeof(struct vm_area));
       return 0;
   }
   else if(addr < now->vm_start && max > now->vm_end){
      struct vm_area* next_temp = next;
      u64 traversed  = 0;
      
      if(next_temp == NULL){
         now->vm_next = next_temp;
         now->vm_end  = addr;
      }
      else{
          int check = 1; 
          while(traversed < (u64)length_mine){
              if(max <= next_temp->vm_end && max >= next_temp->vm_start){
                  traversed =  (u64)length_mine; 
                 //  now -> vm_end = addr;
                  next_temp -> vm_start = max; 
                  if(max == next_temp->vm_end){
                      now -> vm_next = next_temp->vm_next;
                      check = 0;
                      os_free(next_temp, sizeof(struct vm_area));
                     }
                 }
                 
                 else if(max < next_temp->vm_end && max < next_temp->vm_start){
                     traversed =  (u64)length_mine; 
                     // now -> vm_end = addr;
                 }
                 
                 else{
                     struct vm_area* dummy = next_temp->vm_next;
                     
                     if(dummy->vm_start <= max){
                         traversed = (dummy)->vm_start - addr;
                         os_free(next_temp, sizeof(struct vm_area));
                         next_temp = dummy;
                     }
                     else{
                         traversed =  (u64)length_mine; 
                         check = 0;
                         os_free(next_temp, sizeof(struct vm_area));
                     }
                 }
             }
             if(check == 1){
                 prev->vm_next = next_temp;
             }
         }
         
         //  now->vm_next = NULL;
          os_free(now, sizeof(struct vm_area));
         return 0;
   }
   
   else{
       return -1;
   }
   
   //os_free((void*)addr, sizeof(struct vm_area));
}

/**
 * Function will invoked whenever there is page fault for an address in the vm area region
 * created using mmap
 */

long vm_area_pagefault(struct exec_context *current, u64 addr, int error_code){
    struct vm_area* now = current -> vm_area; 
    struct vm_area* next = now -> vm_next;
    struct vm_area* prev = now ;
    find_node(prev, now, next, addr);

    if(MMAP_AREA_START <= addr <= MMAP_AREA_END){ 
        
        if(error_code == 0x4 || error_code == 0x6){
    
            if(current->vm_area->access_flags == 0x1 && error_code == 0x6){
                return -1;
            }
            else{
                if(phy_addr(current -> pgd, addr) == 0){ 
                    // os_pfn_alloc(OS_PT_REG);
                    return create_page((u64*)osmap(current -> pgd), addr, current -> vm_area -> access_flags);
                }
            }
    
        }
        else if(error_code == 0x7){
            // vm_area access flag is either write or read & write

            if(current->vm_area->access_flags == 0x2 || current->vm_area->access_flags == 0x3){
                return handle_cow_fault(current, addr, current -> vm_area -> access_flags);
            }
            else{
                return -1;
            }
            
        }

        else{
            return -1;
        }

    }
    else{
        return -1;
    }

}
/**
 * cfork system call implemenations
 * The parent returns the pid of child process. The return path of
 * the child process is handled separately through the calls at the
 * end of this function (e.g., setup_child_context etc.)
 */

long do_cfork()
{
    u32 pid;
    struct exec_context *new_ctx = get_new_ctx();
    struct exec_context *ctx = get_current_ctx();
    /* Do not modify above lines
     *
     * */
    /--------------------- Your code [start]---------------/
           
            
             new_ctx -> type = ctx -> type;
             new_ctx -> state = ctx -> state;
             new_ctx -> used_mem = ctx -> used_mem;
             new_ctx -> state = ctx -> state;     /Can be any of the states mentioned in schedule.h/
             new_ctx -> used_mem = ctx -> used_mem;
             
             new_ctx -> os_stack_pfn = ctx -> os_stack_pfn;  /Must be unique for every context/
             new_ctx -> os_rsp = ctx -> os_rsp;
             
             for(u64 i = 0 ; i < MAX_MM_SEGS ; i++) new_ctx -> mms[i] = ctx -> mms[i] ;
             struct vm_area * start = ctx -> vm_area ;
             struct vm_area * new_start = new_ctx -> vm_area = (struct vm_area*) os_alloc(sizeof(struct vm_area)) ;
             for(u64 i = 0 ; i < CNAME_MAX ; i++) new_ctx -> name[i] = ctx -> name[i] ;

             
             struct user_regs regs;          /Saved copy of user registers/
             u32 pending_signal_bitmap;      /Pending signal bitmap/
             void* sighandlers[MAX_SIGNALS]; /Signal handler pointers to functions (in user space)/
             u32 ticks_to_sleep;    /Remaining ticks before sleep expires/
             u32 alarm_config_time;   /Alarm ticks set by alarm() system call/
             u32 ticks_to_alarm;   /Remaining ticks before raising SIGALRM/
             struct file* files[MAX_OPEN_FILES]; /*To keep record of openfiles */
             struct ctx_thread_info *ctx_threads;
    new_ctx -> ppid = ctx -> pid ;
    

    /--------------------- Your code [end] ----------------/

    /*
     * The remaining part must not be changed
     */
    copy_os_pts(ctx->pgd, new_ctx->pgd);
    do_file_fork(new_ctx);
    setup_child_context(new_ctx);
    return pid;
}

/* Cow fault handling, for the entire user address space
 * For address belonging to memory segments (i.e., stack, data)
 * it is called when there is a CoW violation in these areas.
 *
 * For vm areas, your fault handler 'vm_area_pagefault'
 * should invoke this function
 * */

long handle_cow_fault(struct exec_context *current, u64 addr, int access_flags){

    u64* pgd_vir = (u64*)osmap(current -> pgd);
    u64 pgd_entry, pud_entry, pmd_entry, pte_entry;
    u64 *pud_vir, *pmd_vir, *pte_vir;
    u64 offset    = addr & 0xFFF;

    u64 pgd_index = (addr >> 39) & 0x1FF;
    u64 pud_index = (addr >> 30) & 0x1FF;
    u64 pmd_index = (addr >> 21) & 0x1FF;
    u64 pte_index = (addr >> 12) & 0x1FF;

    pgd_entry = pgd_vir[pgd_index];
    if (!(pgd_entry & PAGE_PRESENT))
        return -1;

    pud_vir = (u64*)osmap((pgd_entry & PFN_MASK) >> 12);
    pud_entry = pud_vir[pud_index];
    if (!(pud_entry & PAGE_PRESENT)) {
        return -1;
    }

    pmd_vir = (u64*)osmap((pud_entry & PFN_MASK) >> 12);
    pmd_entry = pmd_vir[pmd_index];
    if (!(pmd_entry & PAGE_PRESENT)) {
        return -1;
    }

    pte_vir = (u64*)osmap((pmd_entry & PFN_MASK) >> 12);
    pte_entry = pte_vir[pte_index];
    if (!(pte_entry & PAGE_PRESENT)) {
        return -1;
    }

    u64 old_pfn = pte_entry & PFN_MASK;
  
    // u64 old_pfn = phy_addr((u64)osmap(current -> pgd), addr) >> 12;

    if(get_pfn_refcount(old_pfn) > 1){

        u64 new_pfn = os_pfn_alloc(USER_REG);
        if (!new_pfn){
            return -1;
        }
        memcpy(osmap(new_pfn), osmap(old_pfn), 4096);

        // Update page table entry with new pfn + proper flags
        pte_vir[pte_index] = ((new_pfn << 12) & PAGE_FRAME_MASK) | (PAGE_PRESENT | PAGE_RW | PAGE_USER);

        put_pfn(old_pfn);
        get_pfn(new_pfn);
    }  
  
  
    return 1;
}