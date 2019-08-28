#ifndef _DATA_CENTRIC_H
#define _DATA_CENTRIC_H


#include "config.h"
#include <stdlib.h>
#include <stdint.h>
#include "range.h"
#include "context.h"
#include "stacktraces.h"

#ifndef DATA_CENTRIC
#error "Please don't include this header file."
#endif


class DataCentric {
public:
  static void init(ASGCT_FN asgct);
  static void shutdown();

  static void malloc_hook_function(void *ptr, size_t size);


private:
  static ASGCT_FN _asgct;

  static RangeSet<uint64_t, Context *> _allocated_range_set; //Range-> malloc-context
};
#endif
