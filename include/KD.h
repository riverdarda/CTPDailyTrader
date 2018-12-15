/**
 * @file   KD.h
 * @author alex <yeshiwei.math@gmail.com>
 * @date   Tue Apr 29 21:18:15 2014
 * 
 * @brief  Stochastic Oscillator
 * 
 * 
 */

#ifndef __KD_H__
#define __KD_H__

#include "EMA.h"
#include "maxminwindow.h"

template <typename T>
class KD {
 public:
 KD(size_t kl,
     size_t dl )
   :k_length( kl ),
    d_length( dl ),
    MMW(kl),
    D(dl, 1) {
  }

  void push_back(const T &x);
  
  T get_K() const {
    return 100 * ( price - MMW.min_val() ) 
      / ( MMW.max_val() - MMW.min_val() );
  }

  T get_K( const T p ) const {
    return 100 * ( p - MMW.min_val() ) 
      / ( MMW.max_val() - MMW.min_val() );
  }

  T get_D() const {
    D.last_average();
  }
  
  T size() const {
    return MMW.get_data().size();
  }
  
  T vary() const {
    return MMW.max_val() - MMW.min_val();
  }

 private:
  MaxMinWindow<T> MMW;
  EMA<T> D;
  T price;
  size_t k_length;
  size_t d_length;
};

template <typename T>
void KD<T>::push_back(const T &x) {
  price = x;
  MMW.add_new(x);
  if ( MMW.max_val() != MMW.min_val() ) {
    D.push_back( get_K() );
  }
}

#endif // __KD_H__
