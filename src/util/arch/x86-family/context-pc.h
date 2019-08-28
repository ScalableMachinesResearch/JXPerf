#include <stdio.h>
#include <setjmp.h>
#include <stdbool.h>
#include <assert.h>

#include <sys/types.h>
#include <ucontext.h>

//***************************************************************************
// local include files 
//***************************************************************************
#include "mcontext.h"

//****************************************************************************
// forward declarations 
//****************************************************************************

//****************************************************************************
// interface functions
//****************************************************************************

void* getContextPC(void* uCtxt) {
  mcontext_t *mc = GET_MCONTEXT(uCtxt);
  return MCONTEXT_PC(mc);
}
