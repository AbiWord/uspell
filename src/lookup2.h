// lookup2.h, by Bob Jenkins, December 1996, Public Domain.

#ifndef LOOKUP2_H
#define LOOKUP2_H

typedef  __uint32_t  ub4;   /* unsigned 4-byte quantities */
typedef  unsigned       char ub1;

ub4 hash(register ub1 *k, register ub4 length, register ub4 initval);
ub4 hash2(register const ub4 *k, register ub4 length, register ub4 initval);

#endif
