#define _XOPEN_SOURCE
#define _POSIX_C_SOURCE 199506L

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>


#include "lengths.h"
#include "utils.h"

void lengthsInit(lengthsType *l)
{
  l->size = 0;
  l->len = NULL;
  l->freq = NULL;
  l->lastpos = 0;
  l->sum = 0;
  l->min = (size_t)-1;
  l->max = 0;
}

void lengthsFree(lengthsType *l)
{
  if (l->len) free(l->len);
  l->len = NULL;
  if (l->freq) free(l->freq);
  l->freq = NULL;
}

void lengthsAdd(lengthsType *l, const size_t len, size_t freq)
{
  if (len > (1L << 32) -1) {
    fprintf(stderr,"*error* block length is too large (%zd)\n", len);
    exit(1);
  }
  if (freq < 1) freq = 1;
  l->size++;
  l->len = realloc(l->len, (l->size) * sizeof(lengthsType));
  l->freq = realloc(l->freq, (l->size) * sizeof(lengthsType));
  l->len[l->size - 1] = len;
  l->freq[l->size - 1] = freq;
  if (len > l->max) l->max = len;
  if (len < l->min) l->min = len;
  l->sum += freq;
  //  fprintf(stderr,"add %zd freq %zd sum %zd\n", len, freq, l->sum);
}

size_t lengthsSize(const lengthsType *l)
{
  return l->size;
}

size_t lengthsGet(const lengthsType *l, unsigned int *seed)
{
  if (l->size == 0) {
    return 0;
  } else if (l->size == 1) {
    return l->len[0];
  }
  size_t pos = rand_r(seed) % l->sum;
  size_t inc = 0;
  for (size_t i = 0; i < l->size; i++) {
    inc += l->freq[i];
    //    fprintf(stderr,"*llooking for %zd sum %zd, i = %zd inc %zd\n", pos, l->sum, i, inc);
    if (pos < inc) {
      return l->len[i];
    }
  }
  abort();
  return 0;
  //  return l->len[pos];
}

size_t lengthsMin(const lengthsType *l)
{
  return l->min;
}

size_t lengthsMax(const lengthsType *l)
{
  return l->max;
}


void lengthsSetupLowHighAlignSeq(lengthsType *l, size_t min, size_t max, size_t align)
{
  for (size_t i = min; i <= max; i += align) {
    lengthsAdd(l, alignedNumber(i, align), 1);
  }
}

void lengthsSetupLowHighAlignPower(lengthsType *l, size_t min, size_t max, size_t align)
{
  if (align) {}
  for (size_t i = min; i <= max; i = i*2) {
    lengthsAdd(l, i, 1);
  }
}

void lengthsDump(const lengthsType *l)
{
  fprintf(stderr,"*info* lengths dump (min %zd, max %zd)\n", l->min, l->max);
  for (size_t i = 0; i < l->size; i++) {
    fprintf(stderr,"*info*   [%zd] %zd, %zd\n", i, l->len[i], l->freq[i]);
  }
}

/*int main() {
  lengthsType l;
  unsigned int seed = 0;

  lengthsInit(&l);

  lengthsSetupLowHighAlignPower(&l, 4096, 70000, 4096);

  while (1) {
    fprintf(stderr,"%zd\n", lengthsGet(&l, &seed));
  }
  exit(0);

}

*/
