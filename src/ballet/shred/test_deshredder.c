#include "fd_shred.h"

#include <errno.h>
#include <stdio.h>
#include "../../util/archive/fd_ar.h"

FD_IMPORT_BINARY( localnet_shreds_0,  "src/ballet/shred/fixtures/localnet-slot0-shreds.ar"  );
FD_IMPORT_BINARY( localnet_batch_0_0, "src/ballet/shred/fixtures/localnet-slot0-batch0.bin" );

int
main( int     argc,
      char ** argv ) {
  fd_boot( &argc, &argv );

  /* Open shreds for reading */
  FILE * file = fmemopen( (void *)localnet_shreds_0, localnet_shreds_0_sz, "r" );
  FD_TEST( file );
  FD_TEST( fd_ar_open( file ) );

  /* Concatenate shreds with `fd_deshredder_t` */
  char batch[ 6000 ];      /* Deserialized shred batch */
  fd_deshredder_t deshred; /* Deshredder intermediate state */

  /* Initialize deshredder with empty list of shreds */
  fd_deshredder_init( &deshred, &batch, sizeof(batch), NULL, 0 );

  /* Feed deshredder one-by-one.
     Production code would feed it multiple shreds in batches. */
  fd_ar_t hdr;
  while( fd_ar_next( file, &hdr ) ) {
    uchar shred_buf[ FD_SHRED_SZ ];

    /* Read next file from archive */
    long shred_sz = fd_ar_filesz( &hdr );
    FD_TEST(( shred_sz>=0 && (ulong)shred_sz<sizeof(shred_buf) ));

    size_t n = fread( shred_buf, 1, (ulong)shred_sz, file );
    FD_TEST(( n==(ulong)shred_sz ));

    /* Parse shred */
    fd_shred_t const * shred = fd_shred_parse( shred_buf, (ulong)shred_sz );

    /* Refill deshredder with shred */
    fd_shred_t const * const shred_list[1] = { shred };
    deshred.shreds    = shred_list;
    deshred.shred_cnt = 1U;

    fd_deshredder_next( &deshred );
  }

  /* Did we gracefully finish consuming the archive? */
  FD_TEST( errno==ENOENT );

  /* Check size of defragmented batch */
  ulong batch_sz = sizeof(batch) - deshred.bufsz;
  /* fwrite( batch, batch_sz, 1, stdout ); */
  FD_TEST( batch_sz==3080UL                );
  FD_TEST( batch_sz==localnet_batch_0_0_sz );

  /* Check number of shreds */
  ulong shred_cnt = *(ulong *)batch;
  FD_TEST( shred_cnt==64UL );

  /* Verify deshredded content */
  FD_TEST( 0==memcmp( batch, localnet_batch_0_0, localnet_batch_0_0_sz ) );

  FD_LOG_NOTICE(( "pass" ));
  fd_halt();
  return 0;
}
