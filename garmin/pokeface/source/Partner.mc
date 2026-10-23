import Toybox.Lang;

// Who keeps you company: pure rules, no device reads (Tests.mc covers them).
//   Each weekday has a pair: its day Pokémon from sunrise to sunset, its night Pokémon after.
//   Earned (a run finished today, or steps >= goal): the weekday's reward legendary until midnight.
// Species indices follow tools/gen_pokeface.py's SPECIES (resources Mon0..Mon20).
module Partner {
    // Monday to Sunday: day, night, reward
    const NAMES = ["BLAZIKEN", "SCEPTILE", "LATIAS", "HO-OH", "LUGIA", "SUICUNE",
                   "RAYQUAZA", "DEOXYS", "LATIOS", "CRESSELIA", "DARKRAI", "JIRACHI",
                   "ARCEUS", "GIRATINA", "DIALGA", "SOLGALEO", "LUNALA", "NECROZMA",
                   "XERNEAS", "YVELTAL", "ZYGARDE"];

    enum { HP_NONE, HP_GREEN, HP_YELLOW, HP_RED }

    // CIQ's day_of_week (1 = Sunday ... 7 = Saturday) -> 0 = Monday ... 6 = Sunday.
    function row(dayOfWeek as Number) as Number {
        return (dayOfWeek + 5) % 7;
    }

    // Day is sunrise to sunset; without sun times (no weather position yet), 06:00 to 18:00.
    function isDay(now as Number, hour as Number, rise as Number or Null, set as Number or Null) as Boolean {
        if (rise == null || set == null) {
            return hour >= 6 && hour < 18;
        }
        return now >= rise && now < set;
    }

    function who(dayOfWeek as Number, day as Boolean, earned as Boolean) as Number {
        return row(dayOfWeek) * 3 + (earned ? 2 : (day ? 0 : 1));
    }

    // Body Battery as Emerald's HP bar colour.
    function hpColour(bb as Number or Null) as Number {
        if (bb == null) {
            return HP_NONE;
        }
        return bb > 50 ? HP_GREEN : (bb > 20 ? HP_YELLOW : HP_RED);
    }

    // A goal of 0 or none never counts: no reward for free.
    function earned(steps as Number or Null, goal as Number or Null, ranToday as Boolean) as Boolean {
        if (ranToday) {
            return true;
        }
        return steps != null && goal != null && goal > 0 && steps >= goal;
    }

    // A run counts on the day it finished (start + duration, seconds). Some watches report the
    // start in Garmin's FIT epoch (1989) instead of Unix time: anything 10+ years old is moved up.
    function finishedToday(start as Number, duration as Number, now as Number, todayStart as Number) as Boolean {
        if (start < now - 315360000) {
            start += 631065600;
        }
        return start + duration >= todayStart;
    }

    // The wrist-raise animation's frames, at most: about 10 s at 125 ms, the time the watch stays
    // awake. It stops here even if the watch never says it went back to sleep.
    const MAX_FRAMES = 80;

    function keepAnimating(frames as Number) as Boolean {
        return frames < MAX_FRAMES;
    }

    function dayKey(y as Number, m as Number, d as Number) as Number {
        return y * 10000 + m * 100 + d;
    }

    function barFraction(steps as Number or Null, goal as Number or Null) as Float {
        if (steps == null || goal == null || goal <= 0) {
            return 0.0;
        }
        var f = steps.toFloat() / goal;
        return f > 1.0 ? 1.0 : f;
    }
}
