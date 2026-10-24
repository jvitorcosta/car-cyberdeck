import Toybox.Activity;
import Toybox.ActivityMonitor;
import Toybox.Lang;
import Toybox.SensorHistory;
import Toybox.Time;
import Toybox.UserProfile;
import Toybox.Weather;

// The device's numbers for the face. Each returns null when the watch doesn't have it (no phone,
// no weather sync, no reading yet), and the face shows "--".
module Stats {
    function heartRate() as Number or Null {
        var info = Activity.getActivityInfo();
        if (info != null && info.currentHeartRate != null) {
            return info.currentHeartRate;
        }
        var it = ActivityMonitor.getHeartRateHistory(1, true);
        var s = it != null ? it.next() : null;
        if (s != null && s.heartRate != null && s.heartRate != ActivityMonitor.INVALID_HR_SAMPLE
                && Partner.fresh(s.when != null ? s.when.value() : null, Time.now().value(), 900)) {
            return s.heartRate;   // a reading older than 15 minutes (watch off the wrist) shows as "--"
        }
        return null;
    }

    // The activity history as [type, start, duration s, metres] rows (Runs reads them).
    function activities() as Array<Array> {
        var rows = [] as Array<Array>;
        if (!(Toybox has :UserProfile) || !(UserProfile has :getUserActivityHistory)) {
            return rows;
        }
        var it = UserProfile.getUserActivityHistory();
        var a = it != null ? it.next() : null;
        while (a != null) {
            if (a.startTime != null) {
                rows.add([a.type, a.startTime.value(), a.duration != null ? a.duration.value() : 0,
                          a.distance != null ? a.distance : 0]);
            }
            a = it.next();
        }
        return rows;
    }

    // The past days (their 00:00, epoch seconds; the watch keeps about 7) whose steps reached the goal.
    function stepGoalDays() as Array<Number> {
        var days = [] as Array<Number>;
        if (!(ActivityMonitor has :getHistory)) {
            return days;
        }
        var h = ActivityMonitor.getHistory();
        for (var i = 0; h != null && i < h.size(); i++) {
            var d = h[i];
            if (d != null && d.startOfDay != null && Partner.earned(d.steps, d.stepGoal, false)) {
                days.add(d.startOfDay.value());
            }
        }
        return days;
    }

    function bodyBattery() as Number or Null {
        if (!(Toybox has :SensorHistory) || !(SensorHistory has :getBodyBatteryHistory)) {
            return null;
        }
        var it = SensorHistory.getBodyBatteryHistory({:period => 1, :order => SensorHistory.ORDER_NEWEST_FIRST});
        var s = it != null ? it.next() : null;
        return s != null && s.data != null ? (s.data as Numeric).toNumber() : null;
    }

    function weather() as Weather.CurrentConditions or Null {
        return Weather.getCurrentConditions();
    }

    function today() as ActivityMonitor.Info {
        return ActivityMonitor.getInfo();
    }

    // [sunrise, sunset, tomorrow's sunrise] as epoch seconds, from where the weather was observed.
    function sun(cond as Weather.CurrentConditions or Null) as Array {
        var pos = cond != null ? cond.observationLocationPosition : null;
        if (pos == null) {
            return [null, null, null];
        }
        var today = Time.today();
        var rise = Weather.getSunrise(pos, today);
        var set = Weather.getSunset(pos, today);
        var next = Weather.getSunrise(pos, today.add(new Time.Duration(86400)));
        return [rise != null ? rise.value() : null, set != null ? set.value() : null,
                next != null ? next.value() : null];
    }
}
