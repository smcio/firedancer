#ifndef HEADER_fd_src_ballet_block_fd_block_h
#define HEADER_fd_src_ballet_block_fd_block_h

/* Blocks are the logical representation of Solana block data.

   They consist of 64 entries, each containing a vector of txs. */

#include "../fd_ballet_base.h"
#include "../sha256/fd_sha256.h"

struct __attribute__((packed)) fd_entry {
  /* Number of PoH hashes between this entry and last entry */
  ulong hash_cnt;

  /* PoH state of last entry */
  uchar hash[ FD_SHA256_HASH_SZ ];

  /* Number of transactions in this entry */
  ulong txn_cnt;

  /* Serialized transactions */
  uchar txn_data[];
};
typedef struct fd_entry fd_entry_t;

#endif /* HEADER_fd_src_ballet_block_fd_block_h */
