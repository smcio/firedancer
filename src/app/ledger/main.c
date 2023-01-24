#include <getopt.h>
#include <stdio.h>
#include <stddef.h>
#include <alloca.h>
#include <zstd.h>      // presumes zstd library is installed
#include "../../util/fd_util.h"

static void usage(const char* progname) {
  fprintf(stderr, "USAGE: %s\n", progname);
  fprintf(stderr, "  --snapshotfile (-s)        Input snapshot file\n");
}

struct SnapshotParser {
    size_t totsize_;
};

void SnapshotParser_init(struct SnapshotParser* self) {
  self->totsize_ = 0;
}

void SnapshotParser_moreData(void* arg, const void* data, size_t datalen) {
  struct SnapshotParser* self = (struct SnapshotParser*)arg;
  (void)data;
  self->totsize_ += datalen;
}

typedef void (*decompressCallback)(void* arg, const void* data, size_t datalen);
static void decompressFile(const char* fname, decompressCallback cb, void* arg) {
  FILE* const fin  = fopen(fname, "rb");
  if (fin == NULL) {
    FD_LOG_CRIT(( "unable to read file: %s", fname ));
  }
  size_t const buffInSize = ZSTD_DStreamInSize();
  void*  buffIn = alloca(buffInSize);
  size_t const buffOutSize = ZSTD_DStreamOutSize();  /* Guarantee to successfully flush at least one complete compressed block in all circumstances. */
  void* buffOut = alloca(buffOutSize);

  ZSTD_DCtx* const dctx = ZSTD_createDCtx();
  if (dctx == NULL) {
    FD_LOG_CRIT(( "ZSTD_createDCtx() failed!"));
  }

  /* This loop assumes that the input file is one or more concatenated zstd
   * streams. This example won't work if there is trailing non-zstd data at
   * the end, but streaming decompression in general handles this case.
   * ZSTD_decompressStream() returns 0 exactly when the frame is completed,
   * and doesn't consume input after the frame.
   */
  size_t const toRead = buffInSize;
  size_t read;
  size_t lastRet = 0;
  while ( (read = fread(buffIn, 1, toRead, fin)) ) {
    ZSTD_inBuffer input = { buffIn, read, 0 };
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
      (*cb)(arg, buffOut, output.pos);
      lastRet = ret;
    }
  }


  if (lastRet != 0) {
    /* The last return value from ZSTD_decompressStream did not end on a
     * frame, but we reached the end of the file! We assume this is an
     * error, and the input was truncated.
     */
    FD_LOG_CRIT(( "EOF before end of stream: %zu", lastRet ));
  }

  ZSTD_freeDCtx(dctx);
  fclose(fin);
}

int main(int argc, char** argv) {
  const char* snapshotfile = NULL;
  static struct option arguments[] = {
    {"snapshotfile", required_argument, 0, 's'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
  };
  int option_index = 0, ret = -2;
  while(-1 != (ret = getopt_long(argc, argv, "s:h", arguments, &option_index))) {
    switch (ret) {
    case 's':
      if (optarg != NULL)
        snapshotfile = optarg;
      break;
    case 'h':
    default:
      usage(argv[0]);
      return 1;
    }
  }
  if (snapshotfile == NULL) {
    usage(argv[0]);
    return 1;
  }

  struct SnapshotParser parser;
  SnapshotParser_init(&parser);
  decompressFile(snapshotfile, SnapshotParser_moreData, &parser);
  printf("decompressed %lu bytes\n", parser.totsize_);
  
  return 0;
}
