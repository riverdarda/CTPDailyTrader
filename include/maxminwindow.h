#ifndef __MAXMINWINDOW_H__
#define __MAXMINWINDOW_H__
//Required library
#include <math.h>
// #include <deque>


//A O(sqrt(n))-time O(n)-space algorithm for window data max/min maintain in a data flow
// if need, can be extended to a O(log(n))-time O(n)-space algorithm
template <typename T>
class MaxMinWindow
{
public:
  //constructor
  MaxMinWindow(size_t ws)
    : data(0),
      max_size(ws),
      cache_size(size_t(sqrt(ws))),
      cache_pos(-1),
      cache_min(0),
      cache_max(0),
      allmin(-1),
      allmax(-1)
  {}

  //copy constructor
  MaxMinWindow(const MaxMinWindow &mmw)
    : data(mmw.data),
      max_size(mmw.max_size),
      cache_size(mmw.cache_size),
      cache_pos(mmw.cache_pos),
      cache_min(mmw.cache_min),
      cache_max(mmw.cache_max),
      allmin(mmw.allmin),
      allmax(mmw.allmax)
  {}

  MaxMinWindow& operator=(const MaxMinWindow &mmw)
  {
    data = mmw.data;
    max_size = mmw.max_size;
    cache_size = mmw.cache_size;
    cache_pos = mmw.cache_pos;
    cache_min = mmw.cache_min;
    cache_max = mmw.cache_max;
    allmin = mmw.allmin;
    allmax = mmw.allmax;
    return (*this);
  }

  //clean all cache
  void clear()
  {
    data.clear();
    cache_min.clear();
    cache_max.clear();
    cache_pos = -1;
    allmin = -1;
    allmax = -1;
  }

  //main operation of MaxMinWindow
  //  1.insert a data into datawindow
  //  2.remove oldest data if datawindow is full
  //  3.update max and min of datawindow
  void add_new(const T &x);

  // shift some value for entire class
  void shift(const T &x);

  //get constant reference of min value in datawindow
  inline const T& min_val() const {return data[allmin];}

  //get constant reference of max value in datawindow
  inline const T& max_val() const {return data[allmax];}

  inline int min_pos() const {return allmin;}

  inline int max_pos() const {return allmax;}

  //get constant reference of given data elem, just like use of deque<T>
  inline const T& operator[](size_t i) const {return data[i];}

  //get constant reference of cached datawindow
  inline const std::deque<T>& get_data() const {return data;}

private:
  std::deque<T> data;
  size_t max_size;
  size_t cache_size;

  //internal cache
  int cache_pos;
  std::deque<int> cache_min;
  std::deque<int> cache_max;
  int allmin;
  int allmax;
};

template <typename T>
void MaxMinWindow<T>::shift(const T &x)
{
  for (unsigned i = 0; i < data.size(); i++){
        data[i] += x;
  }
}

template <typename T>
void MaxMinWindow<T>::add_new(const T &x)
{
  data.push_back(x);
  if (data.size() >= cache_size && cache_pos >= 0) {
    size_t tmp_endpos = cache_pos + cache_size * cache_min.size();
    //create new cache set
    if (tmp_endpos + cache_size <= data.size()) {
      int tmp_min = tmp_endpos;
      int tmp_max = tmp_endpos;
      for (size_t i = tmp_endpos + 1; i < tmp_endpos + cache_size; ++i) {
        if (data[i] <= data[tmp_min]) tmp_min = i;
        if (data[tmp_max] <= data[i]) tmp_max = i;
      }
      cache_min.push_back(tmp_min-tmp_endpos);
      cache_max.push_back(tmp_max-tmp_endpos);
    }

    //remove old element
    if (data.size() > max_size) {
      data.pop_front();
      --allmin;
      --allmax;
      if ((--cache_pos) < 0) {
        cache_min.pop_front();
        cache_max.pop_front();
        cache_pos += cache_size;
      }
    }

    //update allmin & allmax
    if (allmin >= 0 && data.back() <= data[allmin]) {
      allmin = data.size() - 1;
    }
    else if (allmin < 0) {
      allmin = 0;
      for (size_t i = 0; i < cache_pos; ++i) {
        if (data[i] <= data[allmin]) allmin = i;
      }
      for (size_t j = 0; j < cache_min.size(); ++j) {
        int tmp_realpos = cache_pos + j * cache_size + cache_min[j];
        if (data[tmp_realpos] <= data[allmin]) allmin = tmp_realpos;
      }
      for (size_t i = cache_pos + cache_size * cache_min.size(); i < data.size(); ++i) {
        if (data[i] <= data[allmin]) allmin = i;
      }
    }

    if (allmax >= 0 && data[allmax] <= data.back()) {
      allmax = data.size() - 1;
    }
    else if (allmax < 0) {
      allmax = 0;
      for (size_t i = 0; i < cache_pos; ++i) {
        if (data[allmax] <= data[i]) allmax = i;
      }
      for (size_t j = 0; j < cache_max.size(); ++j) {
        int tmp_realpos = cache_pos + j * cache_size + cache_max[j];
        if (data[allmax] <= data[tmp_realpos]) allmax = tmp_realpos;
      }
      for (size_t i = cache_pos + cache_size * cache_max.size(); i < data.size(); ++i) {
        if (data[allmax] <= data[i]) allmax = i;
      }
    }

  }
  else {
    if (allmin < 0) allmin = 0;
    if (allmax < 0) allmax = 0;
    if (data.back() <= data[allmin]) allmin = data.size() - 1;
    if (data[allmax] <= data.back()) allmax = data.size() - 1;
    if(data.size() >= cache_size && cache_pos < 0) {
      cache_pos = 0;
      cache_min.push_back(allmin);
      cache_max.push_back(allmax);
    }
  }
}




#endif // __MAXMINWINDOW_H__

