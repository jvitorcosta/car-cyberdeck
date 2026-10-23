// Unit tests for Partner and Fmt (no device data). Build with --unit-test and run with
// monkeydo <prg> fr255 /t (Windows; -t elsewhere) while the simulator is open.
import Toybox.Activity;
import Toybox.Lang;
import Toybox.Test;
import Toybox.Time;
import Toybox.Time.Gregorian;
import Toybox.Weather;

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
    Test.assertEqual(Icons.group(Weather.CONDITION_SAND), Icons.SAND);
    Test.assertEqual(Icons.group(Weather.CONDITION_DUST), Icons.SAND);
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

// Each weekday's pair, Monday to Sunday: day, night, reward (CIQ day_of_week: 1 = Sunday).
(:test)
function testWho(logger as Test.Logger) as Boolean {
    Test.assertEqual(Partner.who(2, true, false), 0);    // Monday day: Blaziken
    Test.assertEqual(Partner.who(2, false, false), 1);   // Monday night: Sceptile
    Test.assertEqual(Partner.who(2, true, true), 2);     // Monday reward: Latias
    Test.assertEqual(Partner.who(1, true, false), 18);   // Sunday day: Xerneas
    Test.assertEqual(Partner.who(7, false, false), 16);  // Saturday night: Lunala
    return true;
}

// After midnight it's already the new weekday: its night Pokémon until sunrise.
(:test)
function testWhoAfterMidnight(logger as Test.Logger) as Boolean {
    var now = 7200;   // 02:00
    var day = Partner.isDay(now, 2, 21600, 64800);
    Test.assertEqual(Partner.who(3, day, false), 4);     // Tuesday night: Lugia
    return true;
}

(:test)
function testIsDayFallback(logger as Test.Logger) as Boolean {
    Test.assert(Partner.isDay(0, 12, null, null));
    Test.assert(Partner.isDay(0, 6, null, null));
    Test.assert(!Partner.isDay(0, 19, null, null));
    Test.assert(!Partner.isDay(0, 5, null, null));
    return true;
}

(:test)
function testIsDaySun(logger as Test.Logger) as Boolean {
    Test.assert(Partner.isDay(3000, 12, 1000, 5000));
    Test.assert(!Partner.isDay(500, 12, 1000, 5000));
    Test.assert(!Partner.isDay(5000, 12, 1000, 5000));
    return true;
}

(:test)
function testHpColour(logger as Test.Logger) as Boolean {
    Test.assertEqual(Partner.hpColour(51), Partner.HP_GREEN);
    Test.assertEqual(Partner.hpColour(50), Partner.HP_YELLOW);
    Test.assertEqual(Partner.hpColour(21), Partner.HP_YELLOW);
    Test.assertEqual(Partner.hpColour(20), Partner.HP_RED);
    Test.assertEqual(Partner.hpColour(0), Partner.HP_RED);
    Test.assertEqual(Partner.hpColour(null), Partner.HP_NONE);
    return true;
}

(:test)
function testNames(logger as Test.Logger) as Boolean {
    Test.assertEqual(Partner.NAMES.size(), 21);
    Test.assert((Partner.NAMES[0] as String).equals("BLAZIKEN"));
    Test.assert((Partner.NAMES[3] as String).equals("HO-OH"));
    Test.assert((Partner.NAMES[20] as String).equals("ZYGARDE"));
    return true;
}

// The weekday is spelled from a fixed English table: the watch's language may need letters the
// Emerald font lacks (Portuguese "SÁB").
(:test)
function testWeekday(logger as Test.Logger) as Boolean {
    Test.assertEqual(Fmt.weekday(1), "SUN");
    Test.assertEqual(Fmt.weekday(2), "MON");
    Test.assertEqual(Fmt.weekday(7), "SAT");
    return true;
}

// Half the round screen's width at a height, and whether a centred line fits between two heights.
(:test)
function testFit(logger as Test.Logger) as Boolean {
    Test.assertEqual(Fit.halfChord(130), 130);
    Test.assertEqual(Fit.halfChord(10), 50);       // sqrt(130^2 - 120^2)
    Test.assertEqual(Fit.halfChord(-5), 0);
    Test.assert(Fit.fits(178, 223, 242, 4) == false);   // a 178 px row near the bottom edge
    Test.assert(Fit.fits(122, 223, 242, 4));
    Test.assert(Fit.fits(150, 196, 214, 4));
    return true;
}

// Runs: a day is 86400 s; "today" starts at T and the week (Monday) at T - 3 days.
const T = 1792713600;
const S = Activity.SPORT_RUNNING;

(:test)
function testSummaryLatestRun(logger as Test.Logger) as Boolean {
    var week = T - 3 * 86400;
    var acts = [[S, T + 3600, 1800, 5000], [S, T + 7200, 2400, 8200], [S, week + 3600, 3000, 10000]];
    var r = Runs.summary(acts, T + 36000, T, week);
    Test.assertEqual(r[0], true);
    Test.assertEqual(r[1], 8200);
    Test.assertEqual(r[2], 2400);
    Test.assertEqual(r[3], 23200);
    return true;
}

(:test)
function testSummaryEmpty(logger as Test.Logger) as Boolean {
    var r = Runs.summary([], T + 36000, T, T - 3 * 86400);
    Test.assertEqual(r[0], false);
    Test.assert(r[1] == null);   // assertEqual can't take null
    Test.assert(r[2] == null);
    Test.assertEqual(r[3], 0);
    return true;
}

(:test)
function testSummaryIgnoresOtherSports(logger as Test.Logger) as Boolean {
    var r = Runs.summary([[Activity.SPORT_CYCLING, T + 3600, 3600, 30000]], T + 36000, T, T - 3 * 86400);
    Test.assertEqual(r[0], false);
    Test.assertEqual(r[3], 0);
    return true;
}

(:test)
function testSummaryFitEpoch(logger as Test.Logger) as Boolean {
    var r = Runs.summary([[S, T + 3600 - 631065600, 1800, 5000]], T + 36000, T, T - 3 * 86400);
    Test.assertEqual(r[0], true);
    Test.assertEqual(r[3], 5000);
    return true;
}

(:test)
function testWeekStartMonday(logger as Test.Logger) as Boolean {
    Test.assertEqual(Partner.weekStart(T, 2), T);              // Monday
    Test.assertEqual(Partner.weekStart(T, 1), T - 6 * 86400);  // Sunday
    return true;
}

(:test)
function testLevelAndExp(logger as Test.Logger) as Boolean {
    Test.assertEqual(Runs.level(23200), 23);
    Test.assertEqual(Runs.level(0), 0);
    Test.assertEqual(Runs.expFraction(25000), 0.5);
    Test.assertEqual(Runs.expFraction(80000), 1.0);
    return true;
}

(:test)
function testPace(logger as Test.Logger) as Boolean {
    Test.assertEqual(Runs.pace(5000, 1560, true), "5:12/km");
    Test.assertEqual(Runs.pace(1609, 502, false), "8:22/mi");
    return true;
}

(:test)
function testPaceZero(logger as Test.Logger) as Boolean {
    Test.assertEqual(Runs.pace(0, 600, true), "--");
    Test.assertEqual(Runs.pace(5000, 0, true), "--");
    Test.assertEqual(Runs.pace(null, null, true), "--");
    return true;
}

(:test)
function testBadge(logger as Test.Logger) as Boolean {
    Test.assertEqual(Partner.badge(15, true, false, 90), Partner.BADGE_PSN);
    Test.assertEqual(Partner.badge(16, false, false, 90), Partner.BADGE_PAR);
    Test.assertEqual(Partner.badge(16, true, true, 25), Partner.BADGE_SLP);
    Test.assertEqual(Partner.badge(16, true, false, 25), Partner.BADGE_NONE);
    Test.assertEqual(Partner.badge(16, true, true, null), Partner.BADGE_NONE);
    return true;
}

(:test)
function testDexIgnoresLastWeek(logger as Test.Logger) as Boolean {
    var week = T - 3 * 86400;
    Test.assertEqual(Partner.dexCount([week - 86400, week, week, T], week, T), 2);
    return true;
}

(:test)
function testAppearUpgrade(logger as Test.Logger) as Boolean {
    var a = Partner.appear(0, true, false);
    Test.assertEqual(a[0], Partner.APPEAR_WILD);
    Test.assertEqual(a[1], 1);
    a = Partner.appear(1, true, false);
    Test.assertEqual(a[0], Partner.APPEAR_NONE);
    a = Partner.appear(1, true, true);
    Test.assertEqual(a[0], Partner.APPEAR_SHINY);
    Test.assertEqual(a[1], 2);
    a = Partner.appear(2, true, true);
    Test.assertEqual(a[0], Partner.APPEAR_NONE);
    a = Partner.appear(0, false, false);
    Test.assertEqual(a[0], Partner.APPEAR_NONE);
    Test.assertEqual(a[1], 0);
    a = Partner.appear(0, true, true);
    Test.assertEqual(a[0], Partner.APPEAR_SHINY);
    Test.assertEqual(a[1], 2);
    return true;
}

(:test)
function testFresh(logger as Test.Logger) as Boolean {
    Test.assert(Partner.fresh(1000, 1800, 900));
    Test.assert(!Partner.fresh(1000, 1901, 900));
    Test.assert(!Partner.fresh(null, 1000, 900));
    return true;
}

// Which days (their 00:00) this week had a run finish: for the Pokédex.
(:test)
function testRunDays(logger as Test.Logger) as Boolean {
    var week = T - 3 * 86400;
    var acts = [[S, T + 3600, 1800, 5000], [S, T - 86400 + 3600, 1800, 5000], [S, week - 3600, 1800, 5000],
                [Activity.SPORT_CYCLING, T - 2 * 86400 + 3600, 1800, 9000]];
    var d = Runs.runDays(acts, T + 36000, T, week);
    Test.assertEqual(d.size(), 2);
    Test.assert(d.indexOf(T) >= 0);
    Test.assert(d.indexOf(T - 86400) >= 0);
    return true;
}

// The two day sources may mark a day a few hours apart (step history at UTC midnight, runs at
// local midnight): days are bucketed to the nearest local midnight, not compared exactly.
(:test)
function testDexBucketsNearbyMidnights(logger as Test.Logger) as Boolean {
    var week = T - 3 * 86400;
    Test.assertEqual(Partner.dexCount([T - 3 * 3600, T], week, T), 1);        // today, from both sources
    Test.assertEqual(Partner.dexCount([week - 3 * 3600], week, T), 1);        // Monday, stamped at UTC midnight
    Test.assertEqual(Partner.dexCount([week - 86400 - 3 * 3600], week, T), 0); // last Sunday stays out
    return true;
}

// A watch that reports run distance with decimals still gives whole metres.
(:test)
function testSummaryWholeMetres(logger as Test.Logger) as Boolean {
    var r = Runs.summary([[S, T + 3600, 1800, 5000.6]], T + 36000, T, T - 3 * 86400);
    Test.assertEqual(r[3], 5000);
    Test.assertEqual(r[1], 5000);
    return true;
}
