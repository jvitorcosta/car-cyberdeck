import Toybox.Lang;
import Toybox.Math;
import Toybox.Time;
import Toybox.Time.Gregorian;

// How each value reads on the face. Anything unknown is "--", never a crash.
module Fmt {
    function num(v as Number or Null) as String {
        return v == null ? "--" : v.toString();
    }

    function clock(hour as Number, min as Number, is24 as Boolean) as String {
        if (is24) {
            return hour.format("%02d") + ":" + min.format("%02d");
        }
        var h = hour % 12;
        return (h == 0 ? 12 : h).toString() + ":" + min.format("%02d");
    }

    // c in °C (as the weather reports it); shown in °F when the watch isn't metric.
    function temp(c as Numeric or Null, metric as Boolean) as String {
        if (c == null) {
            return "--";
        }
        var v = metric ? c.toFloat() : c.toFloat() * 9 / 5 + 32;
        return Math.round(v).toNumber().toString() + "°";
    }

    function dist(cm as Number or Null, metric as Boolean) as String {
        if (cm == null) {
            return "--";
        }
        return metric ? (cm / 100000.0).format("%.1f") + " km" : (cm / 160934.4).format("%.1f") + " mi";
    }

    // Whichever of today's sunrise, today's sunset or tomorrow's sunrise comes next after now
    // (epoch seconds), e.g. "↓18:02".
    function nextSun(now as Number, rise as Number or Null, set as Number or Null, riseTomorrow as Number or Null,
                     is24 as Boolean) as String {
        var t = null;
        var mark = "";
        if (rise != null && rise > now) {
            t = rise;
            mark = "↑";
        } else if (riseTomorrow != null && riseTomorrow > now) {
            t = riseTomorrow;
            mark = "↑";
        }
        if (set != null && set > now && (t == null || set < t)) {
            t = set;
            mark = "↓";
        }
        if (t == null) {
            return "--";
        }
        var i = Gregorian.info(new Time.Moment(t), Time.FORMAT_SHORT);
        return mark + clock(i.hour as Number, i.min as Number, is24);
    }
}
