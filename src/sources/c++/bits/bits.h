/** -*- mode: c++ -*- 
  Licensed under the MIT license: http://www.opensource.org/licenses/mit-license.php 
*/
#ifndef __BITS__BITS_H
#define __BITS__BITS_H
#include <stdint.h>
#include <string>
#include <iostream>

//#include <boost/format.hpp>
//#include <boost/static_assert.hpp>

//#define BITS_T_ASSERT(T) BOOST_STATIC_ASSERT(!std::numeric_limits<T>::is_signed)

namespace bits {
    
  std::string hexdump ( const unsigned char * buffer, int length, int line_size);

  std::string binstr(unsigned char v);
  std::string binstr(const unsigned char * v, std::size_t size);

  template <class T> std::string binstr (T v) {
    std::string s;

#ifdef BITS_LITTLE_ENDIAN
    for (int i=sizeof(v)-1;i>=0;i--) 
#else
      for (int i=0; i < sizeof(v);i++) 
#endif     
	s+=binstr( ((unsigned char *) (&v))[i] );    
    return s;
  }

  unsigned char setbits (unsigned char c, int offset, int numbits, unsigned char v);

  template <class T> void setbitvalue (unsigned char *buffer, T v ,T m ) {
//    BITS_T_ASSERT(T);
    /**
     * Write the value of v in network byte order starting at the location
     * pointed to by buffer, using m as the bitmask.
     * The bitmask determines the bits to be preserved from the initial value
     * of memory at buffer. E.g.
     *             v     =  00001010
     *             m     =  11110001
     * initial buffer[0] =  10010100
     * final   buffer[0] =  10011010
     *                          ^^^  - bits written
     */
    register unsigned shift = sizeof(T)*8; T ff = 0xff;
    for (int j=0;j<sizeof(T);j++) {      
      shift -= 8;
      register T ffs = ff << shift;
      register unsigned char vb = (v  & ffs) >> shift;
      register unsigned char mb = (m  & ffs) >> shift; 
      buffer[j] = (buffer[j] & mb) | vb;
    }
  }

  template <class T> void setbitbuffer (unsigned char *buffer, int offset, std::size_t numbits, T value) {
//    BITS_T_ASSERT(T);
    /**
     * Write the numbits most significant bits of value to buffer, starting at bit offset 
     * in network byte order.
     */
    T v,m;
    unsigned remaining_bits;
    std::size_t size = sizeof(T) * 8;
    
    buffer += (offset/8);  // move to the first affected byte 
    v  = value << (size - numbits) >> (offset % 8);    // shift value to match the bit position
    m  = ~(~((T)0) << (size - numbits) >> (offset%8)); // mask to determine which bits are to be preserved
    setbitvalue (buffer, v, m); // update the first location     
    /* 
     * Now check if the area spans multiple type T locations, and update the remaining bits.
     */
    remaining_bits = ((offset % 8) + numbits) % size; 
    if ((remaining_bits > 0) && ((offset%8) + numbits > size)) {
      v = value << (size - remaining_bits);
      m = ~(~((T)0) << (size - remaining_bits));
      setbitvalue (buffer + sizeof(T),v,m);
    }
  }

//   template <class T> T getbitvalue (unsigned char *buffer, T m) {
// //    BITS_T_ASSERT(T);
//     T rval = 0;
//     unsigned shift = sizeof(T)*8;
//     for (int j=0;j<sizeof(T);j++) {
//       shift -= 8;
//       unsigned char mb = (m  & ((T)0xff << shift)) >> shift;
//       rval |= ((T)buffer[j] & mb) << shift;
//     }
//     return rval;
//   }

//   template <class T> T getbitbuffer (unsigned char *buffer, int offset, std::size_t numbits) {
// //    BITS_T_ASSERT(T);
//     T m, rval;
//     unsigned remaining_bits;
//     std::size_t size = sizeof(T) * 8;
  
//     buffer += (offset/8);
//     m  = (~((T)0) << (size - numbits) >> (offset%8));  
//     rval = getbitvalue (buffer, m); 
//     if ((offset%8) + numbits > size) {
//       remaining_bits = ((offset % 8) + numbits) % size;
//       m =  (~((T)0) << (size - remaining_bits));
//       buffer += sizeof(T);
//       rval <<= remaining_bits;
//       rval += getbitvalue<T>(buffer, m) >> (size - remaining_bits);
//     } else {
//       rval >>= (size - (offset%8) - numbits);
//     }
//     return rval;
//   }
template <class T>
T getbitvalue(const unsigned char *buffer, T m) {
    T rval = 0;
    std::size_t shift = sizeof(T) * 8;

    for (std::size_t j = 0; j < sizeof(T); j++) {
        shift -= 8;
        unsigned char mb = (m & (static_cast<T>(0xff) << shift)) >> shift;
        rval |= (static_cast<T>(buffer[j]) & mb) << shift;
    }
    return rval;
}

template <class T>
T getbitbuffer(const unsigned char *buffer, int offset, std::size_t numbits) {
    if (numbits > sizeof(T) * 8) {
        throw std::invalid_argument("numbits exceeds the size of the type T");
    }

    T m, rval = 0;
    std::size_t remaining_bits;
    std::size_t size = sizeof(T) * 8;

    buffer += (offset / 8);
    m = (~static_cast<T>(0) << (size - numbits) >> (offset % 8));
    rval = getbitvalue(buffer, m);

    if ((offset % 8) + numbits > size) {
        remaining_bits = ((offset % 8) + numbits) % size;
        m = (~static_cast<T>(0) << (size - remaining_bits));
        buffer += sizeof(T);
        rval <<= remaining_bits;
        rval += getbitvalue<T>(buffer, m) >> (size - remaining_bits);
    } else {
        rval >>= (size - (offset % 8) - numbits);
    }

    // 如果 T 是 int64_t，确保 rval 的符号位被正确处理
    if constexpr (std::is_signed<T>::value) {
        T mask = static_cast<T>(1) << (numbits - 1);
        if (rval & mask) {
            rval |= ~((1ULL << numbits) - 1); // 扩展符号位
        }
    }

    return rval;
}
}

#endif
