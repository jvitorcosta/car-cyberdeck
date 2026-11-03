#include "greetpick.h"

namespace greetpick {

struct Entry { Greeting g; When when; };
static const Entry LIST[] = {
    {{u"お帰りなさい", "OKAERINASAI", "WELCOME BACK"}, ANY},
    {{u"ようこそ", "YOKOSO", "WELCOME ABOARD"}, ANY},
    {{u"安全運転で", "ANZEN UNTEN DE", "DRIVE SAFE"}, ANY},
    {{u"よい旅を", "YOI TABI O", "HAVE A GOOD TRIP"}, ANY},
    {{u"出発進行", "SHUPPATSU SHINKO", "LET'S ROLL"}, ANY},
    {{u"おはよう", "OHAYO", "GOOD MORNING"}, MORNING},
    {{u"こんにちは", "KONNICHIWA", "GOOD AFTERNOON"}, DAY},
    {{u"こんばんは", "KONBANWA", "GOOD EVENING"}, EVENING},
    {{u"夜のドライブ", "YORU NO DORAIBU", "NIGHT DRIVE"}, NIGHT},
    {{u"おつかれさま", "OTSUKARESAMA", "GOOD WORK TODAY"}, NIGHT},
};
constexpr int N = sizeof(LIST) / sizeof(LIST[0]);

int count() { return N; }
const Greeting& at(int i) { return LIST[i].g; }
When whenOf(int i) { return LIST[i].when; }

When slot(int m) {
    const int h = m / 60;
    return h >= 5 && h < 11 ? MORNING : h >= 11 && h < 18 ? DAY : h >= 18 && h < 22 ? EVENING : NIGHT;
}

int pick(uint32_t rnd, int last, int minuteOfDay) {
    const When now = minuteOfDay >= 0 ? slot(minuteOfDay) : ANY;
    int list[N];
    int n = 0;
    for (int i = 0; i < N; ++i)
        if (LIST[i].when == ANY || LIST[i].when == now) list[n++] = i;
    if (n > 1) {                       // drop last boot's, when there's another
        int k = 0;
        for (int i = 0; i < n; ++i)
            if (list[i] != last) list[k++] = list[i];
        n = k;
    }
    return list[rnd % (uint32_t)n];
}

}
