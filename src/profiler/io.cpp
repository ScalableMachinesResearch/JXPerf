#include<string.h>
#include<stdlib.h>
#include<fcntl.h>
#include<errno.h>
#include<assert.h>
#include "io.h"
#include "hpcfmt.h"
#include "debug.h"

bool OUTPUT::_openFile() {
  if (_output_buffer.fd >= 0) {
    return true;
  }
  int fd;
  while(1) {
    fd = open(_file_name, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd < 0) {
      if (errno == EEXIST) {
        if (strlen(_file_name) >= NAME_BUFFER_SIZE - 1) {
          ERROR("file_name %s exceeds the maximum length\n",_file_name);
          return false;
        }
        strcat(_file_name, "f");
      } else {
        ERROR("Unable open file %s\n", _file_name);
        return false;
      }
    } else {
      break;
    }
  }
  int ret = hpcio_outbuf_attach(&_output_buffer, fd, _print_buffer, PRINT_BUFFER_SIZE, HPCIO_OUTBUF_UNLOCKED);
  assert(ret == HPCFMT_OK);
  return true;
}

OUTPUT::OUTPUT(char *file_name):OUTPUT(){
  assert(setFileName(file_name));
}

OUTPUT::OUTPUT(){
  _output_buffer.fd = -1;
  memset(_file_name, 0, NAME_BUFFER_SIZE);
}

OUTPUT::~OUTPUT(){
  if (_output_buffer.fd >= 0) {
    hpcio_outbuf_close(&_output_buffer);
  } 
}

bool OUTPUT::setFileName(char *file_name) {
  if (strlen(_file_name) > 0) return false; //already set

  if (strlen(file_name) >= NAME_BUFFER_SIZE - 1) {
    ERROR("given file_name exceeds the maximum length\n");
    return false;
  }
  strcpy(_file_name, file_name);
  return true;
}

int OUTPUT::writef(const char *fmt, ...) {
  #define LOCAL_BUFFER_SIZE 1024
  va_list arg;
  char local_buf[LOCAL_BUFFER_SIZE];

  va_start(arg, fmt);
  int data_size = vsnprintf(local_buf, LOCAL_BUFFER_SIZE, fmt, arg);
  va_end(arg);
  assert(data_size >= 0 && data_size < LOCAL_BUFFER_SIZE );

  return writeb(local_buf);
}

int OUTPUT::writeb(const char *buf) {
  LockScope<SpinLock> lock_scope(&_lock);
  if(_output_buffer.fd < 0) {
    if (strlen(_file_name) == 0) {
      ERROR("Please set file name before writing\n");
      return 0;
    } else {
      if (!_openFile()) {
        return 0;
      }
    }
  }

  int data_size = strlen(buf);
  for(int sent_size = 0; sent_size < data_size;) {
    int ret_size = hpcio_outbuf_write(&_output_buffer, buf + sent_size, data_size - sent_size);
    assert(ret_size >= 0);
    sent_size += ret_size;
  }
  return data_size;
}
