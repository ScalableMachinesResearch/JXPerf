#ifndef _CONTEXT_H
#define _CONTEXT_H
#include <stdint.h>
#include <functional>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include "metrics.h"
#include "stacktraces.h"
#include "xml.h"

#define CONTEXT_TREE_ROOT_ID 0


class ContextFrame {
public:
   ContextFrame& operator=(const ASGCT_CallFrame &asgct_frame);

// fields
   uint64_t binary_addr = 0;
   uint32_t numa_node = 10; 

   jmethodID method_id = 0;
   std::string method_name;
   uint32_t method_version = 0;

   std::string source_file;
   int32_t bci = -1; // bci is not available. We shouldn't initialize bci to 0.
   int32_t src_lineno = 0;
};


// node
class Context {
public:
  Context& operator=(const ContextFrame &frame);
  
  inline void setMetrics(metrics::ContextMetrics *metrics){ _metrics = metrics;}
  inline metrics::ContextMetrics *getMetrics() {return _metrics;}
  inline const ContextFrame &getFrame(){return _frame;}
  std::vector<Context *> getChildren();
  void removeChild(Context *ctxt);
  void addChild(Context *ctxt);
  Context *findChild(Context *ctxt);
  inline Context *getParent() {return _parent;}
  inline bool isTriggered() {return _triggered;}
  inline void setTriggered() {_triggered = true;}


private:
  Context(uint32_t id);
  ~Context();

  // the following two methods are used to distinguish children
  static std::size_t contextRefHash(const Context *ctxt){
    return std::hash<jint>()(ctxt->_frame.bci);
  }
  static bool contextRefEqual(const Context *a, const Context *b){
    return a->_frame.bci == b->_frame.bci &&
      a->_frame.method_id == b->_frame.method_id &&
      a->_frame.binary_addr == b->_frame.binary_addr &&
      a->_frame.numa_node == b->_frame.numa_node &&
      a->_frame.method_version == b->_frame.method_version;
  }

  uint32_t _id;
  bool _triggered = false;
  ContextFrame _frame;
  metrics::ContextMetrics *_metrics = nullptr; 
  
  std::unordered_set<Context*, std::function<std::size_t(Context*)>, std::function<bool(Context*,Context*)> > children;
  Context *_parent = nullptr;

  friend class ContextTree;
  friend xml::XMLObj * xml::createXMLObj<Context>(Context *instance);
};


// tree
class ContextTree {
public:
  ContextTree();
  ~ContextTree();

  Context *addContext(uint32_t parent_id, const ContextFrame &frame);
  Context *addContext(Context *parent, const ContextFrame &frame);
 
  inline Context *getRoot() {return _root;}

  /* iterator */
  typedef typename std::unordered_map<uint32_t,Context *>::iterator MAP_ITERATOR_TYPE;
  class iterator: std::iterator<std::forward_iterator_tag, Context *> {
    public:
    explicit iterator(MAP_ITERATOR_TYPE map_iter):_map_iterator(map_iter){}
    iterator& operator++() { _map_iterator++; return *this;}
    iterator operator++(int){_map_iterator++; return *this;}
    iterator& operator--() = delete;
    iterator operator--(int) = delete;
    bool operator==(iterator other) const {return _map_iterator == other._map_iterator;}
    bool operator!=(iterator other) const {return !(_map_iterator == other._map_iterator);}
    Context* operator*() const { return _map_iterator->second;}
    Context ** operator->() { return &(_map_iterator->second); }

    private:
    MAP_ITERATOR_TYPE _map_iterator;
  };
  iterator begin() { return iterator(_context_map.begin());}
  iterator end() { return iterator(_context_map.end());}

private: 
  std::unordered_map<uint32_t, Context *> _context_map;
  Context *_root;
  uint32_t _id_counter;
};

#endif
