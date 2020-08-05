#include <stdlib.h>
#define UNW_LOCAL_ONLY
#include <libunwind.h>

int get_callinfo(char *fname, size_t fnlen, unsigned long long *ofs)
{
  unw_context_t context;
  unw_cursor_t cursor;
  unw_word_t off, ip, sp;
  unw_proc_info_t pip;
  char procname[256];
  int ret;
  if(unw_getcontext(&context)) {
    return -1;
  }
  if(unw_init_local(&cursor, &context)) {
    return -1;
  }
  if(unw_step(&cursor)<=0) {
    return -1;
  }
  if(unw_step(&cursor)<=0) {
    return -1;
  }
  if(unw_step(&cursor)<=0) {
    return -1;
  }
  if(unw_get_proc_info(&cursor, &pip)) {
    return -1;
  }
  // unw_get_reg(&cursor, UNW_REG_IP, &ip);  
  // *ofs = ip - pip.start_ip;
   
  if(unw_get_proc_name(&cursor, fname, fnlen, &off)) {
    return -1;
  }
  *ofs = off - 5; //5 is the len of the call instruction
  //*ofs = off;
  return 0;
}
