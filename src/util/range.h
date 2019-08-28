#ifndef _RANGE_H
#define _RANGE_H

#include <vector>
#include <map>
#include <iostream>
#include <assert.h>

template<typename T_KEY, typename T_DATA>
class Range {
public:
  T_KEY start;
  T_KEY end;
  T_DATA data;
  Range(const T_KEY &i_start,const T_KEY &i_end, const T_DATA &i_data):
    start(i_start),end(i_end),data(i_data){
    assert(i_start <= i_end);
  }
  Range(){};
};

template<typename T_KEY, typename T_DATA>
class RangeSet;

template<typename T_KEY, typename T_DATA>
std::ostream& operator<< (std::ostream& stream, const RangeSet<T_KEY, T_DATA> &rs){
  for (auto &elem : rs._start_map){
    stream << "["<< elem.first << "," << elem.second.end << "] --> " << elem.second.data << std::endl;
  }
  return stream;
}

template<typename T_KEY, typename T_DATA>
class RangeSet {
public:
  inline bool isEmpty(){ return _start_map.empty();}
  
  bool insert(const T_KEY &start, const T_KEY &end, const T_DATA &data) {
    //TODO: Merge adjacent range with the same data, which can save space
    // if (!isDisjointRange(start, end)) return false;
    _start_map.emplace(start, Range<T_KEY,T_DATA>{start,end,data});
    return true;
  }
  
  bool remove(const T_KEY &start, const T_KEY &end) {
    //return 0, success; -1, does not match start-end pair
    std::vector<Range<T_KEY,T_DATA>> inside_ranges = getRangesInside(start,end);
    if (inside_ranges.size() != 1) {return false;}
    if (inside_ranges[0].start == start && inside_ranges[0].end == end) {
      _start_map.erase(inside_ranges[0].start);
      return true;
    } else {
      return false;
    }
  }
  
  bool remove(const T_KEY &start) {
    //return 0, success; -1, does not found;
    Range<T_KEY,T_DATA> probe_range;
    if (getRange(start, probe_range)) { //found a potential range
      if (probe_range.start == start) {
        _start_map.erase(start);
       return true;
      }
    }
    return false;
  }
 
  bool getRange(const T_KEY &probe_key, Range<T_KEY,T_DATA> &ret_range) {
    if (_start_map.size() == 0) return false;
    auto it = _start_map.upper_bound(probe_key);
    if (it == _start_map.begin()) return false;
    it--;
    if (it->second.end < probe_key) return false;
    ret_range = it->second;
    return true;
  }

  bool getData(const T_KEY &probe_key, T_DATA &data) {
    Range<T_KEY,T_DATA> target_range;
    if (getRange(probe_key, target_range)){ //found
      data = target_range.data;
      return true;
    }
    return false;
  }

  // return all the ranges which can fully fall into [start, end]
  std::vector<Range<T_KEY,T_DATA>> getRangesInside(const T_KEY &start, const T_KEY &end) {
    assert(start <= end);
    std::vector<Range<T_KEY,T_DATA>> ret_vec;
    if (_start_map.size() == 0) return ret_vec;
    auto it = _start_map.lower_bound(start);
    for(;it != _start_map.end();it++){
      if ( it->second.end <= end ){
        ret_vec.push_back(it->second);
      }
      else{
        break;
      }
    }
    return ret_vec;  
  }

  bool isDisjointRange(const T_KEY &start, const T_KEY &end) {
    assert(start <= end);
    if (_start_map.size() == 0) return true;
    
    auto it = _start_map.begin();
    for(; it != _start_map.end(); it++) {
        if ((*it).second.end() < start) continue;
        if ((*it).second.start() > end) return true;
        if ((start <= (*it).second.end() && start >= (*it).second.start()) ||
            ((*it).second.start() <= end && (*it).second.start() >= start))
            return false;
    }
    return true;
  }
      
  //return all the ranges which intersect with [start,end]
  std::vector<Range<T_KEY,T_DATA>> getRangesIntersect(const T_KEY &start, const T_KEY &end) {
    assert(start <= end);
    std::vector<Range<T_KEY,T_DATA>> ret_vec;
    if (_start_map.size() == 0) return ret_vec;
    
    auto it = _start_map.begin();
    for(; it != _start_map.end(); it++) {
        if ((*it).second.end() < start) continue;
        if ((*it).second.start() > end) return ret_vec;
        if ((start <= (*it).second.end() && start >= (*it).second.start()) ||
            ((*it).second.start() <= end && (*it).second.start() >= start))
            ret_vec.push_back((*it).second);
    }
    return ret_vec;
  }
  
  std::vector<Range<T_KEY,T_DATA>> getAllRanges() {
    std::vector<Range<T_KEY,T_DATA>> ret_vec;
    if (_start_map.size() == 0) return ret_vec;
    for(auto &elem : _start_map){
      ret_vec.push_back(elem.second);
    }
    return ret_vec;
  }

  typedef typename std::map<T_KEY, Range<T_KEY,T_DATA>>::iterator MAP_ITERATOR_TYPE;
  class iterator : std::iterator<std::bidirectional_iterator_tag, Range<T_KEY,T_DATA> > {
    public:
    explicit iterator(MAP_ITERATOR_TYPE map_iter) : _map_iterator(map_iter){ }
    iterator& operator++() { _map_iterator++; return *this;}
    iterator operator++(int){_map_iterator++; return *this;}
    iterator& operator--() { _map_iterator--; return *this;}
    iterator operator--(int) { _map_iterator--; return *this;}
    bool operator==(iterator other) const {return _map_iterator == other._map_iterator;}
    bool operator!=(iterator other) const {return !(_map_iterator == other._map_iterator);}
    Range<T_KEY,T_DATA>& operator*() const { return _map_iterator->second;}
    Range<T_KEY,T_DATA> * operator->() { return &(_map_iterator->second); }

    private:
    MAP_ITERATOR_TYPE _map_iterator;
  };

  iterator begin() {return iterator(_start_map.begin());}
  iterator end() {return iterator(_start_map.end());}


  friend std::ostream& operator<< <T_KEY,T_DATA>(std::ostream& stream, const RangeSet<T_KEY, T_DATA> &rs);


private:
  std::map<T_KEY, Range<T_KEY,T_DATA> > _start_map;
};

#endif
