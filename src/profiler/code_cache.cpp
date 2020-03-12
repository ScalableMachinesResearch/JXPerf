#include <assert.h>
#include <iostream>
#include <algorithm>
#include "code_cache.h"
#include "agent.h"
#include "util.h"
#include "thread_data.h" // for testing
#include "profiler.h"
#include "debug.h"

#ifdef PRINT_METHOD_INS
#include <fstream>
#include "x86-misc.h"
#endif

Method::Method(jmethodID method_id):_method_id(method_id) {
  _isNative = JNI_FALSE;
  
  // get the method name
  JvmtiScopedPtr<char> tmp_method_name;
  jvmtiError err = (JVM::jvmti())->GetMethodName(method_id, tmp_method_name.getRef(), NULL, NULL);
  if (err == JVMTI_ERROR_NONE) {
    _method_name = tmp_method_name.get();
  } else if (err == JVMTI_ERROR_WRONG_PHASE) {
    ERROR("JVMTI_ERROR_WRONG_PHASE from GetMethodName calling 0x%lx for thread %d", (uint64_t) method_id, TD_GET(tid));
  }
  
  jboolean native_flag;
  // get the class info
  jclass declaring_class;
  if (false == JVM::check_jvmti_error((JVM::jvmti())->GetMethodDeclaringClass(method_id, &declaring_class), compose_str("GetMethodDeclaringClass failed for ", method_id, "(",_method_name,")"))) {
    if (true == JVM::check_jvmti_error((JVM::jvmti())->IsMethodNative(method_id, &native_flag), compose_str("IsMethodNative failed for ",method_id,"(",getMethodName(),")"))) {
        _isNative = native_flag;
    }
    loadBCILineTable();
    return;
  }

  // get the source file
  JvmtiScopedPtr<char> source_filename;
  err = JVM::jvmti()->GetSourceFileName(declaring_class, source_filename.getRef());
  if (err == JVMTI_ERROR_NONE) {
    _src_file = source_filename.get();
  } else { 
    //Class information does not include a source file name. This includes cases where the class is an array class or primitive class.
    //seems we don't need to do anything
    // assert(JVM::check_jvmti_error(err, compose_str("GetSourceFileName failed for ", method_id, "(",_method_name,")")));
  }

  // get the class name
  JvmtiScopedPtr<char> declaringClassName;
  err = JVM::jvmti()->GetClassSignature(declaring_class, declaringClassName.getRef(), NULL);
  if (err == JVMTI_ERROR_NONE) {
    _class_name = declaringClassName.get();
    _class_name = _class_name.substr(1, _class_name.length() - 2);
    std::replace(_class_name.begin(), _class_name.end(), '/', '.');
  } else {
    // assert(JVM::check_jvmti_error(err, compose_str("GetClassSignature failed for ", method_id, "(",_method_name,")")));
  }

  if (true == JVM::check_jvmti_error((JVM::jvmti())->IsMethodNative(method_id, &native_flag), compose_str("IsMethodNative failed for ",method_id,"(",getMethodName(),")"))) {
    _isNative = native_flag;
  }

  loadBCILineTable();
}

void Method::loadBCILineTable() {
/*
   typedef struct {
    jlocation start_location;
    jint line_number;
  } jvmtiLineNumberEntry;
*/

  if (_isNative) return;
  //already initialized
  if (_bci2line_rangeset.isEmpty() == false) return;

  jvmtiError err;

  //start_location should be always 0;
  jlocation start_location, end_location;
  err = JVM::jvmti()->GetMethodLocation(getMethodID(), &start_location, &end_location);
  // assert(JVM::check_jvmti_error(err, compose_str("GetMethodLocation ")));
  if (err != JVMTI_ERROR_NONE) {
      // assert(false);
      return;
  }

  jint num_entry;
  JvmtiScopedPtr<jvmtiLineNumberEntry> line_table;
  err = JVM::jvmti()->GetLineNumberTable(getMethodID(),&num_entry, line_table.getRef());
  if (err != JVMTI_ERROR_NONE) {
    // assert(false);
    return;
  }

  //NOTE: I have found that the start_location may be the same, indicating one BCI maps to several source line.
  // Not sure whether it is an error, but it appears quite rarely.
  // So far, all start locations are 0.
  for(int i = 0; i < num_entry; i++){
     jlocation tmp_start, tmp_end;
     tmp_start = line_table.get()[i].start_location;
     if (i < num_entry - 1) {
       tmp_end = line_table.get()[i+1].start_location - 1;
     } else {
       tmp_end = end_location;
     }
     if (tmp_end < tmp_start) continue;

     if (_bci2line_rangeset.insert(tmp_start, tmp_end, line_table.get()[i].line_number) == false) {
       INFO("0x%lx %s has irregular BCI2Line mapping", (uint64_t)getMethodID(), getMethodName().c_str());
       ERROR("it ends at %ld\n", end_location);
       for(int i=0; i < num_entry; i++) {
         ERROR("0x%ld -> %d\n", line_table.get()[i].start_location, line_table.get()[i].line_number);       
       }
     }
  }  
  
}

CompiledMethod::CompiledMethod(jmethodID method_id, uint32_t version, jint code_size, const void* code_addr):Method(method_id) {
  _version = version;
  _code_size = code_size;
  _start_addr = (uint64_t)code_addr;
}

CompiledMethod::~CompiledMethod(){}

bool CompiledMethod::isAddrIn(void *addr) {
  if ((uint64_t) addr >= _start_addr && (uint64_t)addr < _start_addr + _code_size) {
    return true;
  }
  return false;
}

void CompiledMethod::loadAddrLocationMap(const jvmtiAddrLocationMap* map, jint map_length){
  //it automatically construct the addr-source mapping
  /*
  typedef struct {
    const void* start_address;
    jlocation location;
  } jvmtiAddrLocationMap;
  */
  // TODO: Map source line no to native methods;
  if (isNative()) return; 
  if (map_length == 0) return;
  // loadBCILineTable();
  if (_bci2line_rangeset.isEmpty()) return;
  
  uint64_t start_addr, end_addr;
  int32_t lineno;
  start_addr = _start_addr;
  end_addr = (uint64_t)map[0].start_address - 1;
  assert(start_addr <= end_addr);
  //if (start_addr <= end_addr) {
    if(_bci2line_rangeset.getData(map[0].location, lineno) == 0)
        return;
    _addr2line_rangeset.insert(start_addr, end_addr, lineno); 
   //}
  
  // NOTE: map[0].start_address > _start_addr. Therefore, there is some code can't be mapped.
  for(int i = 0; i < map_length - 1; i++) {
    start_addr = (uint64_t)map[i].start_address;
    end_addr = (uint64_t)map[i+1].start_address - 1;
    assert(end_addr >= start_addr);
    assert(end_addr <= _start_addr + _code_size - 1); 
    //NOTE: Assume code_end = _start_addr + _code_size - 1
    // We found that map[last_entry].start_address < code_end OR map[last_entry].start_address = code_end + 1
    // Thus maybe map[last_entry].start_address is just used to identify the ending boundary and we should drop the map[last_entry].bci
    
    // assert(_bci2line_rangeset.getData(map[i].location, lineno));
    // off-by-1 line NO.
    if (_bci2line_rangeset.getData(map[i + 1].location, lineno) == 0)
      return;
    _addr2line_rangeset.insert(start_addr, end_addr, lineno); 
  }
  /*
  start_addr = (uint64_t)map[map_length - 1].start_address;
  end_addr = _start_addr + _code_size - 1;
  if (start_addr <= end_addr) {
    assert(_bci2line_rangeset.getData(map[map_length - 1].location, lineno));
    _addr2line_rangeset.insert(start_addr, end_addr, lineno); 
  }
  */
}

int32_t CompiledMethod::addr2line(const uint64_t addr) {
  int32_t line = 0;
  if (_addr2line_rangeset.isEmpty()) // the address has not been set
    return line;

  if (_addr2line_rangeset.getData(addr, line) == false) {
    // ERROR("Can't locate 0x%lx in method %s [%lu, %lu]\n", addr, _method_name.c_str(), (uint64_t) _start_addr, (uint64_t) _start_addr + _code_size -1 );
    assert((uint64_t) _start_addr <= addr && addr <= (uint64_t)_start_addr + _code_size - 1);
  }
  return line;
}

void CompiledMethod:: getMethodBoundary(void **start_addr, void **end_addr) {
    *start_addr = (void *)_start_addr;
    *end_addr = (void *)(_start_addr + _code_size - 1);
}

namespace xml {
template<>
XMLObj * createXMLObj(CompiledMethod *instance){
  std::ostringstream os;
  XMLObj *obj = new(std::nothrow) XMLObj("method");
  assert(obj);
#define SET_ATTR(xmlobj, key, value) os.str(""); os << value; xmlobj->addAttr(key, os.str());
  SET_ATTR(obj, "id",instance->getMethodID());
  SET_ATTR(obj, "version", instance->_version);
  SET_ATTR(obj, "code_size", instance->_code_size);
  SET_ATTR(obj, "start_addr", instance->_start_addr);  
  SET_ATTR(obj, "name", instance->getMethodName().c_str());
  SET_ATTR(obj, "file", instance->getSourceFile().c_str());
  SET_ATTR(obj, "class", instance->getClassName().c_str());

  if ( !instance->_addr2line_rangeset.isEmpty())
  {
  XMLObj *c_obj = new(std::nothrow) XMLObj("addr2line");
  assert(c_obj);
  auto all_range = instance->_addr2line_rangeset.getAllRanges();
  int i = 0;
  for(auto &elem : all_range){
    XMLObj *cc_obj = new(std::nothrow) XMLObj("range");
    assert(cc_obj);
    SET_ATTR(cc_obj, "start", elem.start);
    SET_ATTR(cc_obj, "end", elem.end);
    SET_ATTR(cc_obj, "data", elem.data);
    c_obj->addChild(i++, cc_obj);
  } 
  obj->addChild(1, c_obj);
  }
  
  if (!instance->_bci2line_rangeset.isEmpty())
  {
  XMLObj *c_obj = new(std::nothrow) XMLObj("bci2line");
  assert(c_obj);
  auto all_range = instance->_bci2line_rangeset.getAllRanges();
  int i = 0;
  for (auto &elem : all_range){
    XMLObj *cc_obj = new(std::nothrow) XMLObj("range");
    assert(cc_obj);
    SET_ATTR(cc_obj, "start", elem.start);
    SET_ATTR(cc_obj, "end", elem.end);
    SET_ATTR(cc_obj, "data", elem.data);
    c_obj->addChild(i++,cc_obj);
  }
  obj->addChild(2, c_obj);
  }
#undef SET_ATTR
  return obj;
}

}


CompiledMethodGroup::CompiledMethodGroup(jmethodID method_id) {
  _method_id = method_id;
}

CompiledMethodGroup::~CompiledMethodGroup() {
  for (auto &elem : _address_method_map){
    delete elem.second;
  }
  _address_method_map.clear();
}

CompiledMethod *CompiledMethodGroup::addMethod(jint code_size, const void *code_addr, jint map_length, const jvmtiAddrLocationMap* map, LockScope<SpinLock> &lock_scope) {
  
  uint32_t version = ++_recent_version;
  uint64_t start_addr = (uint64_t) code_addr;
  assert(_address_method_map.find(start_addr) == _address_method_map.end()); // check whether it has been added
  
  lock_scope.unsetLock();
  CompiledMethod * new_method = new(std::nothrow) CompiledMethod(_method_id, version, code_size, code_addr);
  assert(new_method);
  new_method->loadAddrLocationMap(map, map_length);

#ifdef PRINT_METHOD_INS
  std::ofstream inst_file;
  char file_name[128];
  snprintf(file_name, 128, "instruction-%u", TD_GET(tid));
  inst_file.open(file_name, std::ios::app);
  inst_file << "method id = " << _method_id;   
  inst_file << " method version = " << _recent_version; 
  inst_file << std::endl;
  print_instructions(&inst_file, code_addr, (void *)((uint64_t)code_addr + code_size - 1));
  inst_file.close();
#endif
  
  lock_scope.setLock();
  _address_method_map[start_addr] = new_method;
  return new_method;
}

void CompiledMethodGroup::removeMethodByAddr(const void *code_addr) {
  uint64_t start_addr = (uint64_t)code_addr;
  auto it = _address_method_map.find(start_addr);
  // assert(it != _address_method_map.end()); // it should exist
  if (it != _address_method_map.end()) { 
    delete it->second;
    _address_method_map.erase(it);
  }
}

CompiledMethod * CompiledMethodGroup::getMethodByVersion(uint32_t version) {
  // if version == 0, return the most recent one
  if (version == 0) {
    CompiledMethod *ret_method = nullptr;
    for (auto &elem : _address_method_map) {
      CompiledMethod *method = elem.second;
      if (ret_method == nullptr || ret_method->_version < method->_version) {
        ret_method = method;
      }
    }
    return ret_method;
  }
  for (auto &elem : _address_method_map) {
    if (elem.second->_version == version) {
      return elem.second;
    }
  }
  return nullptr;
}


xml::XMLObj *InterpretMethod::createXMLObj() {
  std::ostringstream os;
  xml::XMLObj *obj = new(std::nothrow) xml::XMLObj("method");
  assert(obj);
#define SET_ATTR(xmlobj, key, value) os.str(""); os << value; xmlobj->addAttr(key, os.str());
  SET_ATTR(obj, "id",getMethodID());
  SET_ATTR(obj, "version", 0);
  SET_ATTR(obj, "code_size", 0);
  SET_ATTR(obj, "start_addr", 0);  
  SET_ATTR(obj, "name", getMethodName().c_str());
  SET_ATTR(obj, "file", getSourceFile().c_str());
  SET_ATTR(obj, "class", getClassName().c_str());

  if (!_bci2line_rangeset.isEmpty()) {
  xml::XMLObj *c_obj = new(std::nothrow) xml::XMLObj("bci2line");
  assert(c_obj);
  auto all_range = _bci2line_rangeset.getAllRanges();
  int i = 0;
  for (auto &elem : all_range) {
    xml::XMLObj *cc_obj = new(std::nothrow) xml::XMLObj("range");
    assert(cc_obj);
    SET_ATTR(cc_obj, "start", elem.start);
    SET_ATTR(cc_obj, "end", elem.end);
    SET_ATTR(cc_obj, "data", elem.data);
    c_obj->addChild(i++,cc_obj);
  }
  obj->addChild(2, c_obj);
  }
#undef SET_ATTR
  return obj;
}


CodeCacheManager::CodeCacheManager() {
}


CodeCacheManager::~CodeCacheManager() {
  //use either _method_id_map or _method_range_set
  for (auto &elem : _method_id_map) {
    delete elem.second;
  }
  _method_id_map.clear();
}

CompiledMethod *CodeCacheManager::addMethod(jmethodID method_id, jint code_size, const void* code_addr, jint map_length, const jvmtiAddrLocationMap* map) {
  LockScope<SpinLock> lock_scope(&_lock);
  auto it = _method_id_map.find(method_id);
  CompiledMethodGroup * method_group;
  if (it == _method_id_map.end()) { //not found
    method_group = new(std::nothrow) CompiledMethodGroup(method_id);
    assert(method_group);
    _method_id_map[method_id] = method_group;
  }
  else{
    method_group = it->second;
  }
  // lock_scope.unsetLock();
  //NOTE: When adding a method, some jvmti functions are called in order to retrieve the method information.
  // However, those functions may only be executed when each thread reaches the safe point.
  // If a thread is currently in signal_handler and needs to query the codecache information.
  // A deadlock can happen. Thus we need to release the lock when adding new methods.
  CompiledMethod * new_method = method_group->addMethod(code_size, code_addr, map_length, map, lock_scope);

  // lock_scope.setLock();
  //NOTE: inserting a new range may result in overlapping with other symbols. Let's just assume it won't happen for the moment
  assert(_method_range_set.insert((uint64_t)code_addr, (uint64_t)code_addr + code_size - 1, new_method));
  return new_method;
}


CompiledMethod *CodeCacheManager::addMethodAndRemoveFromUncompiledSet(jmethodID method_id, jint code_size, const void* code_addr, jint map_length, const jvmtiAddrLocationMap* map) {
  LockScope<SpinLock> lock_scope(&_lock);
  Profiler::getProfiler().getUnCompiledMethodCache().removeMethod(method_id); 
  auto it = _method_id_map.find(method_id);
  CompiledMethodGroup * method_group;
  if (it == _method_id_map.end()){ //not found
    method_group = new(std::nothrow) CompiledMethodGroup(method_id);
    assert(method_group);
    _method_id_map[method_id] = method_group;
  }
  else{
    method_group = it->second;
  }
  // lock_scope.unsetLock();
  //NOTE: When adding a method, some jvmti functions are called in order to retrieve the method information.
  // However, those functions may only be executed when each thread reaches the safe point.
  // If a thread is currently in signal_handler and needs to query the codecache information.
  // A deadlock can happen. Thus we need to release the lock when adding new methods.
  CompiledMethod * new_method = method_group->addMethod(code_size, code_addr, map_length, map, lock_scope);

  // lock_scope.setLock();
  //NOTE: inserting a new range may result in overlapping with other symbols. Let's just assume it won't happen for the moment
  assert(_method_range_set.insert((uint64_t)code_addr, (uint64_t)code_addr + code_size - 1, new_method));
  return new_method;
}

void CodeCacheManager::removeMethod(jmethodID method_id, const void* code_addr) {
  LockScope<SpinLock> lock_scope(&_lock);
  auto it = _method_id_map.find(method_id);
  // assert(it != _method_id_map.end());
  if (it != _method_id_map.end()){
    it->second->removeMethodByAddr(code_addr);
    assert(_method_range_set.remove((uint64_t)code_addr));
  }
}

CompiledMethod *CodeCacheManager::getMethod(uint64_t addr) {
  LockScope<SpinLock> lock_scope(&_lock);
  CompiledMethod *m;
  if(_method_range_set.getData(addr, m)) return m;
  return nullptr;
}

CompiledMethod *CodeCacheManager::getMethod(jmethodID method_id) {
  LockScope<SpinLock> lock_scope(&_lock);
  auto it = _method_id_map.find(method_id);
  if (it == _method_id_map.end()) return nullptr;
  CompiledMethodGroup * method_group = it->second;
  return method_group->getMethodByVersion(0);  
}

CompiledMethod *CodeCacheManager::getMethod(const uint64_t addr, jmethodID &method_id, uint32_t &version) {
  LockScope<SpinLock> lock_scope(&_lock);
  CompiledMethod *m;
  if (_method_range_set.getData(addr, m)) { //found
    method_id = m->getMethodID();
    version = m->getVersion();
    return m;
  }
  return nullptr;
}

void CodeCacheManager::checkAndMoveMethodToUncompiledSet(jmethodID method_id) {
  LockScope<SpinLock> lock_scope(&_lock);
  auto it = _method_id_map.find(method_id);
  if (it == _method_id_map.end()) {
    Profiler::getProfiler().getUnCompiledMethodCache().addMethod(method_id); 
  }
}

bool CodeCacheManager::findMethodByAddr(const uint64_t addr, /*output*/ jmethodID &method_id, /*output*/ uint32_t &version) {
  LockScope<SpinLock> lock_scope(&_lock);
  CompiledMethod *m;
  if (_method_range_set.getData(addr, m)) { //found
    method_id = m->getMethodID();
    version = m->getVersion();
    return true;
  }
  return false;
}
