#ifndef CODEC_H
#define CODEC_H

#define FPZIP_FP_FAST 1
#define FPZIP_FP_SAFE 2
#define FPZIP_FP_EMUL 3
#define FPZIP_FP_INT  4

#ifndef FPZIP_FP
  #error "floating-point mode FPZIP_FP not defined"
#elif FPZIP_FP < 1 || FPZIP_FP > 4
  #error "invalid floating-point mode FPZIP_FP"
#endif

#if FPZIP_FP == FPZIP_FP_INT
// identity map for integer arithmetic
template <typename T, unsigned width>
struct PCmap<T, width, T> {
  typedef T FPZIP_Domain_t;
  typedef T FPZIP_Range_t;
  static const unsigned bits = width;
  static const T        mask = ~T(0) >> (bitsizeof(T) - bits);
  FPZIP_Range_t forward(FPZIP_Domain_t d) const { return d & mask; }
  FPZIP_Domain_t inverse(FPZIP_Range_t r) const { return r & mask; }
  FPZIP_Domain_t identity(FPZIP_Domain_t d) const { return d & mask; }
};
#endif

#define FPZ_MAJ_VERSION 0x0101
#define FPZ_MIN_VERSION FPZIP_FP

#endif
