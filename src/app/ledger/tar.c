#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <malloc.h>
#include <string.h>
#include <dirent.h>
#include "../../util/fd_util.h"
#include "tar.h"
typedef unsigned char bool;
#include "tar_impl.h"

#define TarReadStream_initBufSize (1<<16)

void TarReadStream_init(struct TarReadStream* self) {
  self->totalsize_ = 0;
  self->cursize_ = 0;
  self->buf_ = malloc(TarReadStream_initBufSize);
  self->bufmax_ = TarReadStream_initBufSize;
}

void TarReadStream_moreData(struct TarReadStream* self, const void* data, size_t datalen) {
#define CONSUME_DATA(target, cursize, maxsize)           \
  { size_t newsize = cursize + datalen;                  \
    if (newsize > maxsize) newsize = maxsize;            \
    size_t consumed = (size_t)(newsize - cursize);       \
    memcpy((char*)target + cursize, data, consumed);     \
    cursize = newsize;                                   \
    data = (const char*)data + consumed;                 \
    datalen -= consumed; }

  while (datalen) {
    if (self->totalsize_ == 0) {
      // Reading header
      CONSUME_DATA(self->header_, self->cursize_, BLOCKSIZE);
      if (self->cursize_ < BLOCKSIZE) {
        // Incomplete header
        return;
      }
      union block* blk = (union block*)self->header_;
      if (memcmp(blk->header.magic, TMAGIC " ", sizeof(blk->header.magic)) != 0) {
        FD_LOG_ERR(( "tar file has wrong magic number" ));
      }
      printf("%s\n", blk->header.name);
      size_t entsize = (size_t)strtol(blk->header.size, NULL, 10);
      if (entsize == 0) {
        // No content. Probably a directory.
        self->cursize_ = self->totalsize_ = 0;
        continue;
      }
      // Prepare to read content
      self->cursize_ = 0;
      self->totalsize_ = entsize;
      self->roundedsize_ = (entsize + (BLOCKSIZE-1))&(~((size_t)BLOCKSIZE-1));
      if (self->roundedsize_ > self->bufmax_) {
        free(self->buf_);
        self->buf_ = malloc(self->bufmax_ = self->roundedsize_);
      }

    } else {
      // Read content
      CONSUME_DATA(self->buf_, self->cursize_, self->roundedsize_);
      if (self->cursize_ == self->roundedsize_) {
        // Finished entry
        self->cursize_ = self->totalsize_ = 0;
      }
    }
  }

#undef CONSUME_DATA
}

void TarReadStream_destroy(struct TarReadStream* self) {
  free(self->buf_);
}
