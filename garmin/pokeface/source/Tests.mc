// Unit tests for Partner and Fmt (no device data). Build with --unit-test and run with
// monkeydo <prg> fr255 -t while the simulator is open.
import Toybox.Lang;
import Toybox.Test;
import Toybox.Time;
import Toybox.Time.Gregorian;
import Toybox.Weather;

(:test)
function testStages(logger as Test.Logger) as Boolean {
    Test.assertEqual(Partner.pick(6, false, Partner.MEGA_X), Partner.CHARMANDER);
    Test.assertEqual(Partner.pick(11, false, Partner.MEGA_X), Partner.CHARMANDER);
    Test.assertEqual(Partner.pick(12, false, Partner.MEGA_X), Partner.CHARMELEON);
    Test.assertEqual(Partner.pick(17, false, Partner.MEGA_X), Partner.CHARMELEON);
    Test.assertEqual(Partner.pick(18, false, Partner.MEGA_X), Partner.CHARIZARD);
    Test.assertEqual(Partner.pick(21, false, Partner.MEGA_X), Partner.CHARIZARD);
    return true;
}

(:test)
function testAsleep(logger as Test.Logger) as Boolean {
    Test.assert(Partner.asleep(22));
    Test.assert(Partner.asleep(5));
    Test.assert(!Partner.asleep(6));
    Test.assert(!Partner.asleep(21));
    Test.assertEqual(Partner.pick(23, false, Partner.MEGA_Y), Partner.CHARIZARD);
    Test.assertEqual(Partner.pick(23, true, Partner.MEGA_Y), Partner.MEGA_Y);
    Test.assertEqual(Partner.pick(3, false, Partner.MEGA_X), Partner.CHARIZARD);
    return true;
}

(:test)
function testEarnedOverridesDay(logger as Test.Logger) as Boolean {
    Test.assertEqual(Partner.pick(9, true, Partner.MEGA_X), Partner.MEGA_X);
    return true;
}

(:test)
function testEarned(logger as Test.Logger) as Boolean {
    Test.assert(Partner.earned(10000, 10000, false));
    Test.assert(!Partner.earned(9999, 10000, false));
    Test.assert(Partner.earned(0, 10000, true));
    Test.assert(!Partner.earned(null, null, false));
    return true;
}

(:test)
function testZeroGoalNotEarned(logger as Test.Logger) as Boolean {
    Test.assert(!Partner.earned(500, 0, false));
    return true;
}

(:test)
function testMegaResetsOnNewDay(logger as Test.Logger) as Boolean {
    var m = Partner.megaFor(20261024, [20261023, Partner.MEGA_X], 1);
    Test.assertEqual(m[0], 20261024);
    Test.assertEqual(m[1], Partner.MEGA_Y);
    m = Partner.megaFor(20261023, [20261023, Partner.MEGA_X], 1);
    Test.assertEqual(m[0], 20261023);
    Test.assertEqual(m[1], Partner.MEGA_X);
    m = Partner.megaFor(20261023, null, 0);
    Test.assertEqual(m[0], 20261023);
    Test.assertEqual(m[1], Partner.MEGA_X);
    return true;
}

(:test)
function testDayKey(logger as Test.Logger) as Boolean {
    Test.assertEqual(Partner.dayKey(2026, 10, 23), 20261023);
    return true;
}

(:test)
function testFlame(logger as Test.Logger) as Boolean {
    Test.assertEqual(Partner.flameRadius(100, false), 8);
    Test.assertEqual(Partner.flameRadius(0, false), 2);
    Test.assertEqual(Partner.flameRadius(150, false), 8);
    Test.assertEqual(Partner.flameRadius(null, false), 5);
    Test.assertEqual(Partner.flameRadius(100, true), 2);
    return true;
}

(:test)
function testBarFractionZeroGoal(logger as Test.Logger) as Boolean {
    Test.assertEqual(Partner.barFraction(500, 0), 0.0);
    Test.assertEqual(Partner.barFraction(null, 10000), 0.0);
    Test.assertEqual(Partner.barFraction(15000, 10000), 1.0);
    Test.assertEqual(Partner.barFraction(5000, 10000), 0.5);
    return true;
}

(:test)
function testFmtMissingValues(logger as Test.Logger) as Boolean {
    Test.assertEqual(Fmt.num(null), "--");
    Test.assertEqual(Fmt.temp(null, true), "--");
    Test.assertEqual(Fmt.dist(null, true), "--");
    Test.assertEqual(Fmt.nextSun(0, null, null, null, true), "--");
    Test.assertEqual(Fmt.num(64), "64");
    return true;
}

(:test)
function testFmtClock12h(logger as Test.Logger) as Boolean {
    Test.assertEqual(Fmt.clock(13, 5, false), "1:05");
    Test.assertEqual(Fmt.clock(0, 7, false), "12:07");
    Test.assertEqual(Fmt.clock(13, 5, true), "13:05");
    Test.assertEqual(Fmt.clock(6, 0, true), "06:00");
    return true;
}

(:test)
function testFmtImperial(logger as Test.Logger) as Boolean {
    Test.assertEqual(Fmt.temp(24, false), "75°");
    Test.assertEqual(Fmt.temp(24, true), "24°");
    Test.assertEqual(Fmt.dist(482803, false), "3.0 mi");
    Test.assertEqual(Fmt.dist(480000, true), "4.8 km");
    return true;
}

(:test)
function testNextSun(logger as Test.Logger) as Boolean {
    Test.assert(Fmt.nextSun(500, 1000, 5000, 90000, true).find("↑") == 0);
    Test.assert(Fmt.nextSun(2000, 1000, 5000, 90000, true).find("↓") == 0);
    Test.assertEqual(Fmt.nextSun(6000, 1000, 5000, null, true), "--");
    return true;
}

// After today's sunset the next one is tomorrow's sunrise, not "--".
(:test)
function testNextSunEvening(logger as Test.Logger) as Boolean {
    var tomorrow = 90000;
    var i = Gregorian.info(new Time.Moment(tomorrow), Time.FORMAT_SHORT);
    Test.assertEqual(Fmt.nextSun(6000, 1000, 5000, tomorrow, true), "↑" + Fmt.clock(i.hour as Number, i.min as Number, true));
    return true;
}

(:test)
function testWeatherGroups(logger as Test.Logger) as Boolean {
    Test.assertEqual(Icons.group(Weather.CONDITION_CLEAR), Icons.SUN);
    Test.assertEqual(Icons.group(Weather.CONDITION_MOSTLY_CLEAR), Icons.SUN);
    Test.assertEqual(Icons.group(Weather.CONDITION_PARTLY_CLOUDY), Icons.CLOUD);
    Test.assertEqual(Icons.group(Weather.CONDITION_RAIN), Icons.RAIN);
    Test.assertEqual(Icons.group(Weather.CONDITION_SCATTERED_SHOWERS), Icons.RAIN);
    Test.assertEqual(Icons.group(Weather.CONDITION_THUNDERSTORMS), Icons.STORM);
    Test.assertEqual(Icons.group(Weather.CONDITION_SNOW), Icons.SNOW);
    Test.assertEqual(Icons.group(Weather.CONDITION_FOG), Icons.FOG);
    Test.assertEqual(Icons.group(null), Icons.NONE);
    return true;
}

// A run counts on the day it finished, whatever epoch the watch reports its start in.
(:test)
function testRunFinishedToday(logger as Test.Logger) as Boolean {
    var today = 1792713600;   // 2026-10-23 00:00 UTC
    var now = today + 12 * 3600;
    Test.assert(Partner.finishedToday(today + 7 * 3600, 1800, now, today));
    Test.assert(!Partner.finishedToday(today - 5 * 3600, 1800, now, today));          // yesterday
    Test.assert(Partner.finishedToday(today - 600, 1800, now, today));                // crossed midnight
    Test.assert(Partner.finishedToday(today + 7 * 3600 - 631065600, 1800, now, today)); // FIT epoch
    return true;
}

// The wrist-raise animation stops by itself even if the watch never says it went back to sleep.
(:test)
function testAnimationCap(logger as Test.Logger) as Boolean {
    Test.assert(Partner.keepAnimating(1));
    Test.assert(Partner.keepAnimating(Partner.MAX_FRAMES - 1));
    Test.assert(!Partner.keepAnimating(Partner.MAX_FRAMES));
    return true;
}
