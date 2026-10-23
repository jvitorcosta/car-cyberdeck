import Toybox.Activity;
import Toybox.Lang;
import Toybox.Math;

// João's runs, from the watch's activity history as plain [type, start, duration s, metres] rows:
// pure, so Tests.mc covers it. "Today" and "this week" go by when a run finished.
module Runs {
    const WEEK_GOAL_KM = 50;   // the EXP bar fills toward this many km a week

    // Some watches report a start in Garmin's FIT epoch (1989) instead of Unix time: anything 10+
    // years old is moved up.
    function startSecs(start as Number, now as Number) as Number {
        return start < now - 315360000 ? start + 631065600 : start;
    }

    // [ran today, today's latest run metres, its seconds, this week's metres]
    function summary(acts as Array<Array>, now as Number, todayStart as Number, weekStart as Number) as Array {
        var ran = false;
        var lastEnd = -1;
        var lastM = null;
        var lastS = null;
        var week = 0;
        for (var i = 0; i < acts.size(); i++) {
            var a = acts[i];
            if (a[0] != Activity.SPORT_RUNNING) {
                continue;
            }
            var start = startSecs(a[1] as Number, now);
            var end = start + (a[2] as Number);
            if (start > now) {
                continue;
            }
            if (end >= weekStart) {
                week += a[3] as Number;
            }
            if (end >= todayStart) {
                ran = true;
                if (end > lastEnd) {
                    lastEnd = end;
                    lastM = a[3];
                    lastS = a[2];
                }
            }
        }
        return [ran, lastM, lastS, week];
    }

    function level(weekMeters as Number) as Number {
        return weekMeters / 1000;
    }

    function expFraction(weekMeters as Number) as Float {
        var f = weekMeters / (WEEK_GOAL_KM * 1000.0);
        return f > 1.0 ? 1.0 : f;
    }

    // Minutes:seconds per km (or per mile), e.g. "5:12/km"; "--" without a distance or a time.
    function pace(meters as Number or Null, secs as Number or Null, metric as Boolean) as String {
        if (meters == null || secs == null || meters <= 0 || secs <= 0) {
            return "--";
        }
        var per = Math.round(secs * (metric ? 1000.0 : 1609.344) / meters).toNumber();
        return (per / 60).toString() + ":" + (per % 60).format("%02d") + (metric ? "/km" : "/mi");
    }

    // The run card: "8.2 km 5:12/km".
    function card(meters as Number or Null, secs as Number or Null, metric as Boolean) as String {
        return Fmt.dist(meters != null ? meters * 100 : null, metric) + " " + pace(meters, secs, metric);
    }
}
