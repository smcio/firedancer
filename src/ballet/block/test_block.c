#include "fd_block.h"

#include "../../util/sanitize/fd_sanitize.h"

/* Data layout checks */
//FD_STATIC_ASSERT( sizeof(fd_entry_t)==96UL, alignment );

/* A serialized batch of entries.
   Sourced from the genesis of a `solana-test-validator`. */
FD_IMPORT_BINARY( localnet_batch_0, "src/ballet/shred/fixtures/localnet-slot0-batch0.bin" );

/* Target buffer for storing an `fd_entry_t`.
   In production, this would be a workspace. */
static uchar __attribute__((aligned(FD_ENTRY_ALIGN))) entry_buf[ 0x40000 ];

struct fd_entry_test_vec {
  uchar mixin[ FD_SHA256_HASH_SZ ];
  fd_entry_hdr_t hdr;
};
typedef struct fd_entry_test_vec fd_entry_test_vec_t;

void
test_parse_localnet_batch_0( void ) {
  FD_TEST( localnet_batch_0_sz==3080UL );

  /* Peek the number of entries, which is the first ulong. */
  ulong entry_cnt = *(ulong *)localnet_batch_0;
  FD_TEST( entry_cnt==64UL );

  /* Move past the first ulong to the entries. */
  void * batch_buf = (void *)((uchar *)localnet_batch_0    + 8UL);
  ulong  batch_bufsz =                 localnet_batch_0_sz - 8UL ;

  /* Check whether our .bss buffer fits. */
  ulong const txn_max_cnt = 10;
  ulong footprint = fd_entry_footprint( txn_max_cnt );
  FD_TEST( sizeof(entry_buf)>=footprint );

  /* Mark buffer for storing `fd_entry_t` as unallocated (poisoned).
     `fd_entry_new` will partially unpoison the beginning of this buffer.

     This helps catch OOB accesses beyond the end of `fd_entry_t`. */
  fd_asan_poison( entry_buf, sizeof(entry_buf) );

  /* Create new object in .bss buffer. */
  void * shentry = fd_entry_new( entry_buf, txn_max_cnt );
  FD_TEST( shentry );

  /* Get reference to newly created entry. */
  fd_entry_t * entry = fd_entry_join( shentry );
  FD_TEST( entry );

  /* Deserialize all entries. */
  fd_txn_parse_counters_t counters_opt = {0};
  void * next_buf;
  for( ulong i=0; i<entry_cnt; i++ ) {
    FD_TEST( batch_buf );

    next_buf = fd_entry_deserialize( entry, batch_buf, batch_bufsz, &counters_opt );
    batch_bufsz -= ((ulong)next_buf - (ulong)batch_buf);
    batch_buf    = next_buf;

    /* Each entry in the genesis block has 0 txns, 0 hashes, and the same prev hash */
    FD_TEST( entry->hdr.txn_cnt ==0UL );
    FD_TEST( entry->hdr.hash_cnt==0UL );
    // TODO verify 8246845ac88a7eea46845ac88a7eea04845ac88a7eea04e35ac88a7eea04e325
  }
}

int
main( int     argc,
      char ** argv ) {
  fd_boot( &argc, &argv );

  test_parse_localnet_batch_0();

  FD_LOG_NOTICE(( "pass" ));
  fd_halt();
  return 0;
}
