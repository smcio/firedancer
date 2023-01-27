#include "fd_block.h"

#include "../bmtree/fd_bmtree.h"

FD_FN_CONST ulong
fd_entry_align( void ) {
  return FD_ENTRY_ALIGN;
}

FD_FN_CONST ulong
fd_entry_footprint( ulong txn_cnt ) {
  /* Fixed-size data preceding VLA */
  ulong hdr_sz = sizeof(fd_entry_t);

  /* Max required heap space for all txn descriptors */
  ulong txn_tbl_sz = txn_cnt*FD_TXN_MAX_SZ;

  /* TODO overflow checks */

  return hdr_sz + txn_tbl_sz;
}

void *
fd_entry_new( void * shmem,
                ulong  txn_cnt ) {

  if( FD_UNLIKELY( !shmem ) ) {
    FD_LOG_WARNING(( "NULL shmem" ));
    return NULL;
  }

  if( FD_UNLIKELY( !fd_ulong_is_aligned( (ulong)shmem, fd_entry_align() ) ) ) {
    FD_LOG_WARNING(( "misaligned shmem" ));
    return NULL;
  }

  ulong footprint = fd_entry_footprint( txn_cnt );
  if( FD_UNLIKELY( !footprint ) ) {
    FD_LOG_WARNING(( "bad txn_cnt (%lu)", txn_cnt ));
    return NULL;
  }

  fd_memset( shmem, 0, footprint );

  /* TODO use private header */
  fd_entry_t * hdr = (fd_entry_t *)shmem;

  hdr->txn_max_cnt = txn_cnt;

  FD_COMPILER_MFENCE();
  hdr->magic = FD_ENTRY_MAGIC;
  FD_COMPILER_MFENCE();

  return shmem;
}

fd_entry_t *
fd_entry_join( void * shentry ) {

  if( FD_UNLIKELY( !shentry ) ) {
    FD_LOG_WARNING(( "NULL shentry" ));
    return NULL;
  }

  if( FD_UNLIKELY( !fd_ulong_is_aligned( (ulong)shentry, fd_entry_align() ) ) ) {
    FD_LOG_WARNING(( "misaligned shentry" ));
    return NULL;
  }

  fd_entry_t * hdr = (fd_entry_t *)shentry;
  if( FD_UNLIKELY( hdr->magic!=FD_ENTRY_MAGIC ) ) {
    FD_LOG_WARNING(( "bad magic" ));
    return NULL;
  }

  return (fd_entry_t *)shentry;
}

void *
fd_entry_leave( fd_entry_t * entry ) {

  if( FD_UNLIKELY( !entry ) ) {
    FD_LOG_WARNING(( "NULL entry" ));
    return NULL;
  }

  return (void *)entry;
}

void *
fd_entry_delete( void * shentry ) {

  if( FD_UNLIKELY( !shentry ) ) {
    FD_LOG_WARNING(( "NULL shentry" ));
    return NULL;
  }

  if( FD_UNLIKELY( !fd_ulong_is_aligned( (ulong)shentry, fd_entry_align() ) ) ) {
    FD_LOG_WARNING(( "misaligned shentry" ));
    return NULL;
  }

  fd_entry_t * hdr = (fd_entry_t *)shentry;
  if( FD_UNLIKELY( hdr->magic!=FD_ENTRY_MAGIC ) ) {
    FD_LOG_WARNING(( "bad magic" ));
    return NULL;
  }

  FD_COMPILER_MFENCE();
  FD_VOLATILE( hdr->magic ) = 0UL;
  FD_COMPILER_MFENCE();

  return shentry;
}

void *
fd_entry_deserialize( fd_entry_t * entry,
                      void *       buf,
                      ulong        buf_sz,
                      fd_txn_parse_counters_t * counters_opt ) {
# define ADVANCE(n) do {                         \
  if( FD_UNLIKELY( buf_sz < (n) ) ) return NULL; \
  buf     = (void *)((char *)buf + (n));         \
  buf_sz -= (n);                                 \
} while(0)

  /* Copy entry header */
  fd_entry_hdr_t * hdr = (fd_entry_hdr_t *)buf;
  ADVANCE( sizeof(fd_entry_hdr_t) );
  fd_memcpy( &entry->hdr, hdr, sizeof(fd_entry_hdr_t) );

  /* Parse transactions */
  fd_rawtxn_b_t * raw_tbl = entry->raw_tbl;
  fd_txn_o_t    * txn_tbl = entry->txn_tbl;
  for( ulong txn_idx=0; txn_idx < entry->hdr.txn_cnt; txn_idx++ ) {
    /* Remember ptr of raw txn */
    fd_rawtxn_b_t * raw_entry = &raw_tbl[ txn_idx ];
    void * raw_ptr = buf;

    /* Parse txn into descriptor table */
    fd_txn_t * out_txn = (fd_txn_t *)&txn_tbl[ txn_idx ];
    ulong txn_sz = fd_txn_parse( buf, buf_sz, out_txn, counters_opt );
    FD_LOG_NOTICE(( "fd_txn_parse: %lu", txn_sz ));

    if( FD_UNLIKELY( txn_sz==0UL ) ) return NULL;
    ADVANCE( txn_sz );

    raw_entry->raw    = raw_ptr;
    raw_entry->txn_sz = (ushort)txn_sz;
  }

  return buf;

# undef ADVANCE
}

void
fd_entry_mixin( fd_entry_t const * entry,
                uchar *            out_hash ) {
  ulong txn_cnt = entry->hdr.txn_cnt;

  fd_bmtree32_commit_t * commit =
  fd_alloca( FD_ENTRY_ALIGN, fd_bmtree32_commit_footprint( txn_cnt ) );

  if( FD_UNLIKELY( !commit ) )
    FD_LOG_ERR(( "fd_entry_mixin: fd_alloca for entry with %lu txns failed", txn_cnt ));

  fd_rawtxn_b_t const * raw_tbl = entry->raw_tbl;
  fd_txn_o_t    const * txn_tbl = entry->txn_tbl;

  for( ulong i=0; i<txn_cnt; i++ ) {
    void     const * raw =                    raw_tbl[ i ].raw;
    fd_txn_t const * txn = (fd_txn_t const *)&txn_tbl[ i ];

    fd_ed25519_sig_t const * sigs = fd_txn_signatures( txn, raw );

    fd_bmtree32_node_t leaf[1];
    fd_bmtree32_hash_leaf( leaf[0], &sigs[0], sizeof(fd_ed25519_sig_t) );
    fd_bmtree32_commit_append( commit, (fd_bmtree32_node_t const *)&leaf[0], 1 );
  }

  fd_bmtree32_node_t * root = fd_bmtree32_commit_fini( commit );
  fd_memcpy( out_hash, root, sizeof(fd_bmtree32_node_t) );
}
