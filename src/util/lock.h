#ifndef _LOCK_H
#define _LOCK_H
#include <assert.h>

class SpinLock {
public:
  bool tryLock(){
    return __sync_bool_compare_and_swap(&_lock, 0, 1);
  }
  void lock(){
    while(!tryLock()){
      asm volatile("pause");
    }
  }
  void unlock(){
      // __sync_fetch_and_sub(&_lock, 1);
      // __sync_fetch_and_and(&_lock, 0);
      __sync_bool_compare_and_swap(&_lock, 1, 0);
  }
  bool isLocked(){
    __sync_synchronize();
    bool ret_val = (_lock > 0);
    return ret_val; 
  }
private:
  volatile int _lock = 0;
};


template <typename LockType>
class LockScope {
public:
  LockScope(LockType * lock_ptr):_lock_ptr(lock_ptr){
    _lock_ptr->lock();
  }
  ~LockScope(){
    _lock_ptr->unlock();
  }
  void setLock(){
    _lock_ptr->lock();
  }
  void unsetLock(){
    assert(_lock_ptr->isLocked());
    _lock_ptr->unlock();
  }
private:
  LockType  *_lock_ptr;
};

#endif
