import Toybox.Activity;
import Toybox.ActivityMonitor;
import Toybox.Lang;
import Toybox.SensorHistory;
import Toybox.Time;
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
        if (s != null && s.heartRate != null && s.heartRate != ActivityMonitor.INVALID_HR_SAMPLE) {
            return s.heartRate;
        }
        return null;
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
