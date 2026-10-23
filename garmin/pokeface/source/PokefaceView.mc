import Toybox.Activity;
import Toybox.Application;
import Toybox.Graphics;
import Toybox.Lang;
import Toybox.Math;
import Toybox.System;
import Toybox.Time;
import Toybox.Time.Gregorian;
import Toybox.Timer;
import Toybox.UserProfile;
import Toybox.WatchUi;

// true: the simulator steps through every stage, one per minute (06, 12, 18, Mega X, Mega Y,
// asleep at 23 with Mega X, asleep at 03). Never ship it on.
const DEMO = false;

// Layout "A, all at once", top to bottom: weather + next sunrise/sunset; weekday, date, phone,
// notifications, battery; the time; the partner with heart rate + temperature on its left and
// Body Battery + calories on its right; the step bar; steps and distance.
// Asleep (low power) the partner holds one pose a minute; a wrist raise animates it for the few
// seconds the watch stays awake, except at night.
class PokefaceView extends WatchUi.WatchFace {
    const CX = 130;
    const SPRITE_Y = 150;
    const FRAME_MS = 125;

    private var _sprite as Sprite = new Sprite();
    private var _timer as Timer.Timer or Null;
    private var _frame as Number = 0;
    private var _minute as Number = -1;   // the partner is decided once a minute
    private var _species as Number = Partner.CHARMANDER;
    private var _asleep as Boolean = false;

    function initialize() {
        WatchFace.initialize();
        Math.srand(Time.now().value());
    }

    // The face just appeared (back from a menu or an activity): someone is looking.
    function onShow() as Void {
        animate();
    }

    function onExitSleep() as Void {
        animate();
    }

    private function animate() as Void {
        if (_asleep || _timer != null) {
            return;
        }
        _timer = new Timer.Timer();
        _timer.start(method(:onFrame), FRAME_MS, true);
    }

    function onEnterSleep() as Void {
        if (_timer != null) {
            _timer.stop();
            _timer = null;
        }
        _minute = -1;   // back to the minute's pose
        WatchUi.requestUpdate();
    }

    function onFrame() as Void {
        _frame++;
        WatchUi.requestUpdate();
    }

    function onUpdate(dc as Dc) as Void {
        var set = System.getDeviceSettings();
        var metric = set.temperatureUnits == System.UNIT_METRIC;
        var now = Gregorian.info(Time.now(), Time.FORMAT_MEDIUM);
        var act = Stats.today();
        var cond = Stats.weather();
        dc.setColor(Graphics.COLOR_BLACK, Graphics.COLOR_BLACK);
        dc.clear();

        // Weather, today's high / low, and the next sunrise or sunset
        var sun = Stats.sun(cond);
        var next = Fmt.nextSun(Time.now().value(), sun[0], sun[1], set.is24Hour);
        var hi = cond != null ? Fmt.temp(cond.highTemperature, metric) : "--";
        var lo = cond != null ? Fmt.temp(cond.lowTemperature, metric) : "--";
        var mark = next.substring(0, 1);
        var hasSun = mark.equals("↑") || mark.equals("↓");
        var sunText = hasSun ? next.substring(1, null) : next;
        var row1 = hi + " " + lo + "  ";
        var w1 = dc.getTextWidthInPixels(row1, Graphics.FONT_XTINY);
        var ws = dc.getTextWidthInPixels(sunText, Graphics.FONT_XTINY);
        var x1 = CX - (20 + w1 + (hasSun ? 14 : 0) + ws) / 2;
        Icons.weather(dc, x1 + 8, 24, Icons.group(cond != null ? cond.condition : null));
        text(dc, x1 + 20, 24, row1, Graphics.COLOR_LT_GRAY, Graphics.TEXT_JUSTIFY_LEFT);
        if (hasSun) {
            Icons.sunArrow(dc, x1 + 20 + w1 + 6, 24, mark.equals("↑"));
        }
        text(dc, x1 + 20 + w1 + (hasSun ? 14 : 0), 24, sunText, Graphics.COLOR_LT_GRAY, Graphics.TEXT_JUSTIFY_LEFT);

        // Weekday + date, phone dot, notifications, battery
        var day = (now.day_of_week as String).toUpper() + " " + now.day.toString();
        var bat = System.getSystemStats().battery.toNumber().toString() + "%";
        var notes = set.notificationCount.toString();
        var w2 = dc.getTextWidthInPixels(day, Graphics.FONT_XTINY);
        var wn = dc.getTextWidthInPixels(notes, Graphics.FONT_XTINY);
        var wb = dc.getTextWidthInPixels(bat, Graphics.FONT_XTINY);
        var x2 = CX - (w2 + 14 + 6 + 18 + wn + 10 + wb) / 2;
        text(dc, x2, 48, day, Graphics.COLOR_WHITE, Graphics.TEXT_JUSTIFY_LEFT);
        x2 += w2 + 10;
        dc.setColor(0x00AAFF, Graphics.COLOR_TRANSPARENT);
        if (set.phoneConnected) {
            dc.fillCircle(x2, 48, 3);
        } else {
            dc.drawCircle(x2, 48, 3);
        }
        x2 += 14;
        Icons.envelope(dc, x2, 48);
        text(dc, x2 + 9, 48, notes, Graphics.COLOR_LT_GRAY, Graphics.TEXT_JUSTIFY_LEFT);
        text(dc, x2 + 9 + wn + 10, 48, bat, Graphics.COLOR_LT_GRAY, Graphics.TEXT_JUSTIFY_LEFT);

        // The time
        var clock = System.getClockTime();
        textIn(dc, CX, 90, Fmt.clock(clock.hour, clock.min, set.is24Hour), Graphics.COLOR_WHITE,
             Graphics.TEXT_JUSTIFY_CENTER, Graphics.FONT_NUMBER_MEDIUM);

        // The partner, with heart rate + temperature left and Body Battery + calories right
        drawPartnerAt(dc, CX, SPRITE_Y, clock);
        var hr = Fmt.num(Stats.heartRate());
        var whr = dc.getTextWidthInPixels(hr, Graphics.FONT_XTINY);
        Icons.heart(dc, 92 - whr - 10, 140, 0xFF5555);
        text(dc, 92, 140, hr, Graphics.COLOR_WHITE, Graphics.TEXT_JUSTIFY_RIGHT);
        text(dc, 92, 162, cond != null ? Fmt.temp(cond.temperature, metric) : "--", Graphics.COLOR_WHITE,
             Graphics.TEXT_JUSTIFY_RIGHT);
        text(dc, 168, 140, "BB " + Fmt.num(Stats.bodyBattery()), 0x55AAFF, Graphics.TEXT_JUSTIFY_LEFT);
        text(dc, 168, 162, Fmt.num(act.calories) + " kcal", Graphics.COLOR_WHITE, Graphics.TEXT_JUSTIFY_LEFT);

        // Steps against the goal
        var f = Partner.barFraction(act.steps, act.stepGoal);
        dc.setColor(Graphics.COLOR_DK_GRAY, Graphics.COLOR_TRANSPARENT);
        dc.fillRoundedRectangle(75, 192, 110, 5, 2);
        if (f > 0.0) {
            dc.setColor(0xFFAA00, Graphics.COLOR_TRANSPARENT);
            dc.fillRoundedRectangle(75, 192, (110 * f).toNumber(), 5, 2);
        }
        text(dc, CX, 212, Fmt.num(act.steps) + " · " + Fmt.dist(act.distance, set.distanceUnits == System.UNIT_METRIC),
             Graphics.COLOR_LT_GRAY, Graphics.TEXT_JUSTIFY_CENTER);
    }

    function drawPartnerAt(dc as Dc, cx as Number, cy as Number, clock as System.ClockTime) as Void {
        if (clock.min != _minute) {
            _minute = clock.min;
            decide(clock);
            if (_timer == null) {
                _frame = clock.min;
            }
        }
        _sprite.show(_species, _asleep);
        _sprite.draw(dc, cx, cy, _frame, Partner.flameRadius(Stats.bodyBattery(), _asleep));
        if (_asleep) {
            var c = _sprite.corner();
            text(dc, cx + c[0] + 2, cy + c[1] + 4, "z", Graphics.COLOR_LT_GRAY, Graphics.TEXT_JUSTIFY_LEFT);
            text(dc, cx + c[0] + 10, cy + c[1] - 6, "Z", Graphics.COLOR_LT_GRAY, Graphics.TEXT_JUSTIFY_LEFT);
        }
    }

    // Who is out this minute: by the hour, or the day's Mega once a run or the step goal is done.
    private function decide(clock as System.ClockTime) as Void {
        var hour = clock.hour;
        var act = Stats.today();
        var earned = Partner.earned(act.steps, act.stepGoal, ranToday());
        var mega = Partner.MEGA_X;
        if (DEMO) {
            var stage = [[6, false, 0], [12, false, 0], [18, false, 0], [12, true, 0], [12, true, 1],
                         [23, true, 0], [3, false, 0]][clock.min % 7] as Array;
            hour = stage[0] as Number;
            earned = stage[1] as Boolean;
            mega = stage[2] == 0 ? Partner.MEGA_X : Partner.MEGA_Y;
        } else if (earned) {
            var now = Gregorian.info(Time.now(), Time.FORMAT_SHORT);
            var today = Partner.dayKey(now.year, now.month as Number, now.day);
            var stored = Application.Storage.getValue("mega") as Array or Null;
            var day = Partner.megaFor(today, stored, Math.rand() % 2);
            if (stored == null || stored[0] != today) {
                Application.Storage.setValue("mega", day as Application.PropertyValueType);
            }
            mega = day[1] as Number;
        }
        _asleep = Partner.asleep(hour);
        _species = Partner.pick(hour, earned, mega);
        if (_asleep && _timer != null) {
            _timer.stop();
            _timer = null;
        }
    }

    // A run finished today, from the watch's activity history.
    private function ranToday() as Boolean {
        if (!(UserProfile has :getUserActivityHistory)) {
            return false;
        }
        var it = UserProfile.getUserActivityHistory();
        var start = Time.today().value();
        var a = it != null ? it.next() : null;
        while (a != null) {
            if (a.type == Activity.SPORT_RUNNING && a.startTime != null && a.startTime.value() >= start) {
                return true;
            }
            a = it.next();
        }
        return false;
    }

    // One line of small text, vertically centred on y.
    function text(dc as Dc, x as Number, y as Number, s as String, color as Number, justify as Number) as Void {
        textIn(dc, x, y, s, color, justify, Graphics.FONT_XTINY);
    }

    function textIn(dc as Dc, x as Number, y as Number, s as String, color as Number, justify as Number,
                    font as Graphics.FontType) as Void {
        dc.setColor(color, Graphics.COLOR_TRANSPARENT);
        dc.drawText(x, y, font, s, justify | Graphics.TEXT_JUSTIFY_VCENTER);
    }
}
