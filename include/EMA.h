/**
 * @file   EMA.h
 * @author alex <yeshiwei.math@gmail.com>
 * @date   Tue Apr 29 21:14:06 2014
 * 
 * @brief  Exponential Moving Average
 * 
 * 
 */


#ifndef __EMA_H__
#define __EMA_H__
#include <deque>

template <typename T>
class EMA {
public:
  EMA(size_t al,
     size_t ms)
   :average_length( al ),
    max_size( ms ) {
      alpha = 2.0/( average_length + 1 );
  }

  EMA(size_t al)
   :average_length( al ),
    max_size( 10 ) {
      alpha = 2.0/( average_length + 1 );
  }

  void push_back(const T&x);

  inline T last_average() const {
    return average.back();    
  }
  
  inline T live_average(const T&x) const {
    return alpha * x + ( 1.0 - alpha ) * average.back();
  }

  inline const T& operator[](size_t i) const {return average[i];}

 private:
  std::deque<T> average;
  double alpha;
  size_t max_size;
  size_t average_length;
};

template <typename T>
void EMA<T>::push_back(const T & x) {
  T val;
  if ( average.size() == 0 ) {
    val = x;
  } else {
    val = alpha * x + ( 1.0 - alpha ) * average.back();
  }
  average.push_back( val );
  if ( average.size() > max_size ) average.pop_front();
}

#endif // __EMA_H__
