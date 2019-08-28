#ifndef _IO_H
#define _IO_H

#include "hpcio-buffer.h"
#include "lock.h"
class INPUT {
private:

public:

};

class OUTPUT {
private:
  #define PRINT_BUFFER_SIZE (1<<10)
  #define NAME_BUFFER_SIZE (256)
  SpinLock _lock;
  char _print_buffer[PRINT_BUFFER_SIZE];
  char _file_name[NAME_BUFFER_SIZE];
  hpcio_outbuf_t _output_buffer;

  bool _openFile();

public:
  OUTPUT(char *file_name);
  OUTPUT();
  ~OUTPUT();
  bool setFileName(char *file_name); // return false if it has been set;
  int writef(const char *fmt, ...);
  int writeb(const char *buf);
};

#endif
