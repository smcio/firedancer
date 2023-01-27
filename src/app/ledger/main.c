#include <stdio.h>
#include <stddef.h>
#include <alloca.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <zstd.h>      // presumes zstd library is installed
#include "../../util/fd_util.h"
#include "tar.h"

static void usage(const char* progname) {
  fprintf(stderr, "USAGE: %s\n", progname);
  fprintf(stderr, "  --snapshotfile <file>        Input snapshot file\n");
}

struct Account_StoredMeta {
    unsigned long write_version_obsolete;
    char pubkey[32];
    unsigned long data_len;
};

struct Account_AccountMeta {
    unsigned long lamports;
    char owner[32];
    char executable;
    unsigned long rent_epoch;
};

struct Account_Hash {
    char value[32];
};

struct SnapshotParser {
  struct TarReadStream tarreader_;
};

void SnapshotParser_init(struct SnapshotParser* self) {
  TarReadStream_init(&self->tarreader_);
}

void SnapshotParser_destroy(struct SnapshotParser* self) {
  TarReadStream_destroy(&self->tarreader_);
}

void SnapshotParser_tarEntry(void* arg, const char* name, const void* data, size_t datalen) {
  (void)arg;
  
  if (datalen == 0 || strncmp(name, "accounts/", sizeof("accounts/")-1) != 0)
    return;

  while (datalen) {
    size_t roundedlen;
    
#define EAT_SLICE(_target_, _len_)         \
    roundedlen = (_len_+7UL)&~7UL;         \
    if (roundedlen > datalen) return;      \
    memcpy(_target_, data, _len_);         \
    data = (const char*)data + roundedlen; \
    datalen -= roundedlen;

    struct Account_StoredMeta meta;
    EAT_SLICE(&meta, sizeof(meta));
    struct Account_AccountMeta account_meta;
    EAT_SLICE(&account_meta, sizeof(account_meta));
    struct Account_Hash hash;
    EAT_SLICE(&hash, sizeof(hash));

    // Skip data for now
    roundedlen = (meta.data_len+7UL)&~7UL;
    if (roundedlen > datalen) return;
    data = (const char*)data + roundedlen;
    datalen -= roundedlen;

#undef EAT_SLICE
  }

  printf("%s\n", name);
}

// Return non-zero on end of tarball
int SnapshotParser_moreData(void* arg, const void* data, size_t datalen) {
  struct SnapshotParser* self = (struct SnapshotParser*)arg;
  return TarReadStream_moreData(&self->tarreader_, data, datalen, SnapshotParser_tarEntry, self);
}

typedef int (*decompressCallback)(void* arg, const void* data, size_t datalen);
static void decompressFile(const char* fname, decompressCallback cb, void* arg) {
  int const fin = open(fname, O_RDONLY);
  if (fin == -1) {
    FD_LOG_ERR(( "unable to read file %s: %s", fname, strerror(errno) ));
  }
  size_t const buffInSize = ZSTD_DStreamInSize();
  void*  buffIn = alloca(buffInSize);
  size_t const buffOutSize = ZSTD_DStreamOutSize();  /* Guarantee to successfully flush at least one complete compressed block in all circumstances. */
  void* buffOut = alloca(buffOutSize);

  ZSTD_DCtx* const dctx = ZSTD_createDCtx();
  if (dctx == NULL) {
    FD_LOG_ERR(( "ZSTD_createDCtx() failed!"));
  }

  /* This loop assumes that the input file is one or more concatenated zstd
   * streams. This example won't work if there is trailing non-zstd data at
   * the end, but streaming decompression in general handles this case.
   * ZSTD_decompressStream() returns 0 exactly when the frame is completed,
   * and doesn't consume input after the frame.
   */
  ssize_t readRet;
  size_t lastRet = 0;
  while ( (readRet = read(fin, buffIn, buffInSize)) ) {
    if (readRet == -1) {
      FD_LOG_ERR(( "unable to read file %s: %s", fname, strerror(errno) ));
    }
    ZSTD_inBuffer input = { buffIn, (unsigned)readRet, 0 };
    /* Given a valid frame, zstd won't consume the last byte of the frame
     * until it has flushed all of the decompressed data of the frame.
     * Therefore, instead of checking if the return code is 0, we can
     * decompress just check if input.pos < input.size.
     */
    while (input.pos < input.size) {
      ZSTD_outBuffer output = { buffOut, buffOutSize, 0 };
      /* The return code is zero if the frame is complete, but there may
       * be multiple frames concatenated together. Zstd will automatically
       * reset the context when a frame is complete. Still, calling
       * ZSTD_DCtx_reset() can be useful to reset the context to a clean
       * state, for instance if the last decompression call returned an
       * error.
       */
      size_t const ret = ZSTD_decompressStream(dctx, &output , &input);
      if ((*cb)(arg, buffOut, output.pos)) {
        lastRet = 0;
        break;
      }
      lastRet = ret;
    }
  }


  if (lastRet != 0) {
    /* The last return value from ZSTD_decompressStream did not end on a
     * frame, but we reached the end of the file! We assume this is an
     * error, and the input was truncated.
     */
    FD_LOG_ERR(( "EOF before end of stream: %zu", lastRet ));
  }

  ZSTD_freeDCtx(dctx);
  close(fin);
}

int main(int argc, char** argv) {
  const char* snapshotfile = fd_env_strip_cmdline_cstr(&argc, &argv, "--snapshotfile", NULL, NULL);
  if (snapshotfile == NULL) {
    usage(argv[0]);
    return 1;
  }

  struct SnapshotParser parser;
  SnapshotParser_init(&parser);
  decompressFile(snapshotfile, SnapshotParser_moreData, &parser);
  SnapshotParser_destroy(&parser);
  
  return 0;
}
