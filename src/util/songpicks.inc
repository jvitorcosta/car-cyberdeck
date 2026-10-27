// The owner's picks for music mode (util/songmood): they beat every other rule. Included by
// songmood.cpp. To add one, add a line: the artist (always), then the song's title for a song, or
// nullptr and the album's name for a whole album, or nullptr twice for every song of the artist;
// then the swatch. A song's pick beats its album's, which beats its artist's. Write names as Spotify shows them: case, accents and suffixes such as
// " - Remastered" or " (feat. ...)" don't matter.
static const Pick PICKS[] = {
    // artist         song                    album          swatch
    {"The Weeknd", "I Was Never There", nullptr, RED},
    {"The Weeknd", "Save Your Tears", nullptr, RED},
    {"The Weeknd", "Out of Time", nullptr, BLUE},
    {"The Weeknd", "Blinding Lights", nullptr, RED},
    {"Vulgo FK", nullptr, "F**K SONGS", PINK},   // the pink one ("TÁ TARDE", "Pausa")
    {"Budah", nullptr, nullptr, PURPLE},
};
