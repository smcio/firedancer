#define TAR_BLOCKSIZE 512

struct TarReadStream {
    size_t cursize_;
    size_t totalsize_; // 0 if reading header
    size_t roundedsize_; // 0 if reading header
    char header_[TAR_BLOCKSIZE];
    void* buf_;
    size_t bufmax_;
};

inline size_t TarReadStream_footprint() { return sizeof(struct TarReadStream); }

extern void TarReadStream_init(struct TarReadStream* self);

extern void TarReadStream_moreData(struct TarReadStream* self, const void* data, size_t datalen);

extern void TarReadStream_destroy(struct TarReadStream* self);
