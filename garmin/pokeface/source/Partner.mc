import Toybox.Lang;

// Who keeps you company, and how: pure rules, no device reads (Tests.mc covers them).
//   Awake: Charmander 06-11, Charmeleon 12-17, Charizard 18-21. Asleep 22-05: Charizard.
//   Earned (a run today, or steps >= goal): the day's Mega, awake or asleep, rolled once a day.
module Partner {
    enum { CHARMANDER, CHARMELEON, CHARIZARD, MEGA_X, MEGA_Y }

    function asleep(hour as Number) as Boolean {
        return hour >= 22 || hour < 6;
    }

    // mega is the day's roll (MEGA_X or MEGA_Y), used only when earned.
    function pick(hour as Number, earned as Boolean, mega as Number) as Number {
        if (earned) {
            return mega;
        }
        if (asleep(hour) || hour >= 18) {
            return CHARIZARD;
        }
        return hour >= 12 ? CHARMELEON : CHARMANDER;
    }

    // A goal of 0 or none never counts: no Mega for free.
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

    // stored is yesterday's or today's [dayKey, mega] (or null); a new day takes the new roll (0 = X).
    function megaFor(today as Number, stored as Array or Null, roll as Number) as Array {
        if (stored != null && stored.size() == 2 && stored[0] == today) {
            return stored;
        }
        return [today, roll == 0 ? MEGA_X : MEGA_Y];
    }

    // The tail flame follows Body Battery: 2 px drained, 8 px full; unknown sits in the middle.
    function flameRadius(bb as Number or Null, asleep as Boolean) as Number {
        if (asleep) {
            return 2;
        }
        if (bb == null) {
            return 5;
        }
        var v = bb < 0 ? 0 : (bb > 100 ? 100 : bb);
        return 2 + v * 6 / 100;
    }

    function barFraction(steps as Number or Null, goal as Number or Null) as Float {
        if (steps == null || goal == null || goal <= 0) {
            return 0.0;
        }
        var f = steps.toFloat() / goal;
        return f > 1.0 ? 1.0 : f;
    }
}
