#ifndef _CODEBOOK_H
#define _CODEBOOK_H

#include <fstream>
#include <string>
#include <cstdlib>
#include "errors.h"
#include "Bit_stream.h"

using namespace std;

/* stuff from Tremor (lowmem) */
namespace {
int ilog(unsigned int v){
  int ret=0;
  while(v){
    ret++;
    v>>=1;
  }
  return(ret);
}

unsigned int _book_maptype1_quantvals(unsigned int entries, unsigned int dimensions){
  /* get us a starting hint, we'll polish it below */
  int bits=ilog(entries);
  int vals=entries>>((bits-1)*(dimensions-1)/dimensions);

  while(true){
    unsigned long acc=1;
    unsigned long acc1=1;
	  for(unsigned int i = 0;i<dimensions;i++){
      acc*=vals;
      acc1*=vals+1;
    }
    if(acc<=entries && acc1>entries){
      return(vals);
    }else{
      if(acc>entries){
        vals--;
      }else{
        vals++;
      }
    }
  }
}

}

class codebook_library
{
    char * codebook_data;
    long * codebook_offsets;
    long codebook_count;

    // Intentionally undefined
    codebook_library& operator=(const codebook_library& rhs) = delete;
    codebook_library(const codebook_library& rhs) = delete;

public:
    codebook_library(void);

    ~codebook_library()
    {
        delete [] codebook_data;
        delete [] codebook_offsets;
    }

    const char * get_codebook(int i) const
    {
        if (!codebook_data || !codebook_offsets)
        {
            throw Parse_error_str("codebook library not loaded");
        }
        if (i >= codebook_count-1 || i < 0) return nullptr;
        return &codebook_data[codebook_offsets[i]];
    }

    long get_codebook_size(int i) const
    {
        if (!codebook_data || !codebook_offsets)
        {
            throw Parse_error_str("codebook library not loaded");
        }
        if (i >= codebook_count-1 || i < 0) return -1;
        return codebook_offsets[i+1]-codebook_offsets[i];
    }

    void rebuild(int i, Bit_oggstream& bos);

    void rebuild(Bit_stream &bis, unsigned long cb_size, Bit_oggstream& bos);

    void copy(Bit_stream &bis, Bit_oggstream& bos);
};
#endif
