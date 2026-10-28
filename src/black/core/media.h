#pragma once
#include <cstddef>
#include <cstdint>
#include "../../common/core/embed.h"
// The deck's media (clips, montage, intro video, CJK font...). On the S3 (MEDIA_PARTITION) they
// live in the "media" flash partition, packed by tools/pack_media.py and flashed with
//   pio run -e s3 -t uploadmedia
// so a code change flashes ~1.3 MB instead of the whole 8 MB. Without
// MEDIA_PARTITION they are linked into the app (board_build.embed_files). Either way a call site reads
//   MEDIA_DECL(data_intro_bin);                     at namespace scope
//   media::Blob b = MEDIA(data_intro_bin, "intro.bin");
// A file the pack lacks, a bad CRC or an empty partition reads as {nullptr, 0}.
namespace media {
struct Blob { const uint8_t* data; size_t size; };
// Maps the partition and checks its header, once; logs "[media] N files, ..." or "[media] none".
void begin();
Blob get(const char* name);   // calls begin(); each file's CRC is checked on its first get()
}

#ifdef MEDIA_PARTITION
#define MEDIA_DECL(sym) static_assert(true, "in the media partition")
#define MEDIA(sym, name) (media::get(name))
#else
#define MEDIA_DECL(sym) EMBED(sym)
#define MEDIA(sym, name) (media::Blob{EMBED_DATA(sym), EMBED_SIZE(sym)})
#endif
