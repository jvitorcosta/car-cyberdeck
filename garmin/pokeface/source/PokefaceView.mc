import Toybox.Activity;
import Toybox.ActivityMonitor;
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
// seconds the watch stays awake, except at night. The watch's numbers are read once a minute and
// on waking (refresh); animation frames only redraw them, to spare the battery.
class PokefaceView extends WatchUi.WatchFace {
    const CX = 130;
    const SPRITE_Y = 150;
    const FRAME_MS = 125;

    private var _sprite as Sprite = new Sprite();
    private var _timer as Timer.Timer or Null;
    private var _frame as Number = 0;
    private var _frames as Number = 0;    // frames played since this wake
    private var _minute as Number = -1;   // -1: read everything on the next update
    private var _species as Number = Partner.CHARMANDER;
    private var _asleep as Boolean = false;

    // What refresh() read, ready to draw
    private var _sunMark as String = "";
    private var _sunText as String = "--";
    private var _hiLo as String = "-- --";
    private var _weather as Number = Icons.NONE;
    private var _temp as String = "--";
    private var _day as String = "";
    private var _bat as String = "";
    private var _notes as String = "0";
    private var _phone as Boolean = false;
    private var _hr as String = "--";
    private var _bb as String = "--";
    private var _flame as Number = 5;
    private var _kcal as String = "--";
    private var _stepsLine as String = "--";
    private var _bar as Float = 0.0;

    function initialize() {
        WatchFace.initialize();
        Math.srand(Time.now().value());
    }

    // The face just appeared (back from a menu or an activity): someone is looking.
    function onShow() as Void {
        _minute = -1;
        animate();
    }

    function onExitSleep() as Void {
        _minute = -1;
        animate();
    }

    function onEnterSleep() as Void {
        stopAnimating();
        _minute = -1;   // back to the minute's pose
        WatchUi.requestUpdate();
    }

    function onHide() as Void {
        stopAnimating();
    }

    private function animate() as Void {
        if (_asleep || _timer != null) {
            return;
        }
        _frames = 0;
        _timer = new Timer.Timer();
        _timer.start(method(:onFrame), FRAME_MS, true);
    }

    private function stopAnimating() as Void {
        if (_timer != null) {
            _timer.stop();
            _timer = null;
        }
    }

    function onFrame() as Void {
        _frame++;
        _frames++;
        if (!Partner.keepAnimating(_frames)) {
            stopAnimating();
        }
        WatchUi.requestUpdate();
    }

    function onUpdate(dc as Dc) as Void {
        var clock = System.getClockTime();
        var is24 = System.getDeviceSettings().is24Hour;
        if (clock.min != _minute) {
            _minute = clock.min;
            refresh(clock, is24);
            if (_timer == null) {
                _frame = clock.min;
            }
        }
        dc.setColor(Graphics.COLOR_BLACK, Graphics.COLOR_BLACK);
        dc.clear();

        // Weather, today's high / low, and the next sunrise or sunset
        var hasSun = _sunMark.length() > 0;
        var w1 = dc.getTextWidthInPixels(_hiLo, Graphics.FONT_XTINY);
        var ws = dc.getTextWidthInPixels(_sunText, Graphics.FONT_XTINY);
        var x1 = CX - (20 + w1 + (hasSun ? 14 : 0) + ws) / 2;
        Icons.weather(dc, x1 + 8, 24, _weather);
        text(dc, x1 + 20, 24, _hiLo, Graphics.COLOR_LT_GRAY, Graphics.TEXT_JUSTIFY_LEFT);
        if (hasSun) {
            Icons.sunArrow(dc, x1 + 20 + w1 + 6, 24, _sunMark.equals("↑"));
        }
        text(dc, x1 + 20 + w1 + (hasSun ? 14 : 0), 24, _sunText, Graphics.COLOR_LT_GRAY, Graphics.TEXT_JUSTIFY_LEFT);

        // Weekday + date, phone dot, notifications, battery
        var w2 = dc.getTextWidthInPixels(_day, Graphics.FONT_XTINY);
        var wn = dc.getTextWidthInPixels(_notes, Graphics.FONT_XTINY);
        var wb = dc.getTextWidthInPixels(_bat, Graphics.FONT_XTINY);
        var x2 = CX - (w2 + 14 + 6 + 18 + wn + 10 + wb) / 2;
        text(dc, x2, 48, _day, Graphics.COLOR_WHITE, Graphics.TEXT_JUSTIFY_LEFT);
        x2 += w2 + 10;
        dc.setColor(0x00AAFF, Graphics.COLOR_TRANSPARENT);
        if (_phone) {
            dc.fillCircle(x2, 48, 3);
        } else {
            dc.drawCircle(x2, 48, 3);
        }
        x2 += 14;
        Icons.envelope(dc, x2, 48);
        text(dc, x2 + 9, 48, _notes, Graphics.COLOR_LT_GRAY, Graphics.TEXT_JUSTIFY_LEFT);
        text(dc, x2 + 9 + wn + 10, 48, _bat, Graphics.COLOR_LT_GRAY, Graphics.TEXT_JUSTIFY_LEFT);

        // The time
        textIn(dc, CX, 90, Fmt.clock(clock.hour, clock.min, is24), Graphics.COLOR_WHITE,
               Graphics.TEXT_JUSTIFY_CENTER, Graphics.FONT_NUMBER_MEDIUM);

        // The partner, with heart rate + temperature left and Body Battery + calories right
        drawPartnerAt(dc, CX, SPRITE_Y);
        var whr = dc.getTextWidthInPixels(_hr, Graphics.FONT_XTINY);
        Icons.heart(dc, 92 - whr - 10, 140, 0xFF5555);
        text(dc, 92, 140, _hr, Graphics.COLOR_WHITE, Graphics.TEXT_JUSTIFY_RIGHT);
        text(dc, 92, 162, _temp, Graphics.COLOR_WHITE, Graphics.TEXT_JUSTIFY_RIGHT);
        text(dc, 168, 140, "BB " + _bb, 0x55AAFF, Graphics.TEXT_JUSTIFY_LEFT);
        text(dc, 168, 162, _kcal + " kcal", Graphics.COLOR_WHITE, Graphics.TEXT_JUSTIFY_LEFT);

        // Steps against the goal
        dc.setColor(Graphics.COLOR_DK_GRAY, Graphics.COLOR_TRANSPARENT);
        dc.fillRoundedRectangle(75, 192, 110, 5, 2);
        if (_bar > 0.0) {
            dc.setColor(0xFFAA00, Graphics.COLOR_TRANSPARENT);
            dc.fillRoundedRectangle(75, 192, (110 * _bar).toNumber(), 5, 2);
        }
        text(dc, CX, 212, _stepsLine, Graphics.COLOR_LT_GRAY, Graphics.TEXT_JUSTIFY_CENTER);
    }

    // Reads the watch's numbers and decides who is out; once a minute and on waking.
    private function refresh(clock as System.ClockTime, is24 as Boolean) as Void {
        var set = System.getDeviceSettings();
        var metric = set.temperatureUnits == System.UNIT_METRIC;
        var act = Stats.today();
        var cond = Stats.weather();

        var sun = Stats.sun(cond);
        var next = Fmt.nextSun(Time.now().value(), sun[0], sun[1], sun[2], is24);
        var mark = next.substring(0, 1);
        _sunMark = mark.equals("↑") || mark.equals("↓") ? mark : "";
        _sunText = _sunMark.length() > 0 ? next.substring(1, null) : next;
        _hiLo = (cond != null ? Fmt.temp(cond.highTemperature, metric) : "--") + " "
                + (cond != null ? Fmt.temp(cond.lowTemperature, metric) : "--") + "  ";
        _weather = Icons.group(cond != null ? cond.condition : null);
        _temp = cond != null ? Fmt.temp(cond.temperature, metric) : "--";

        var now = Gregorian.info(Time.now(), Time.FORMAT_MEDIUM);
        _day = (now.day_of_week as String).toUpper() + " " + now.day.toString();
        _bat = System.getSystemStats().battery.toNumber().toString() + "%";
        _notes = set.notificationCount.toString();
        _phone = set.phoneConnected;

        _hr = Fmt.num(Stats.heartRate());
        var bb = Stats.bodyBattery();
        _bb = Fmt.num(bb);
        _kcal = Fmt.num(act.calories);
        _stepsLine = Fmt.num(act.steps) + " · " + Fmt.dist(act.distance, set.distanceUnits == System.UNIT_METRIC);
        _bar = Partner.barFraction(act.steps, act.stepGoal);

        decide(clock, act);
        _flame = Partner.flameRadius(bb, _asleep);
    }

    function drawPartnerAt(dc as Dc, cx as Number, cy as Number) as Void {
        _sprite.show(_species, _asleep);
        _sprite.draw(dc, cx, cy, _frame, _flame);
        if (_asleep) {
            var c = _sprite.corner();
            text(dc, cx + c[0] + 2, cy + c[1] + 4, "z", Graphics.COLOR_LT_GRAY, Graphics.TEXT_JUSTIFY_LEFT);
            text(dc, cx + c[0] + 10, cy + c[1] - 6, "Z", Graphics.COLOR_LT_GRAY, Graphics.TEXT_JUSTIFY_LEFT);
        }
    }

    // Who is out this minute: by the hour, or the day's Mega once a run or the step goal is done.
    private function decide(clock as System.ClockTime, act as ActivityMonitor.Info) as Void {
        var hour = clock.hour;
        var earned = Partner.earned(act.steps, act.stepGoal, false) || ranToday();
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
            if (day != stored) {   // megaFor hands back the stored array when it is still today's
                Application.Storage.setValue("mega", day as Application.PropertyValueType);
            }
            mega = day[1] as Number;
        }
        _asleep = Partner.asleep(hour);
        _species = Partner.pick(hour, earned, mega);
        if (_asleep) {
            stopAnimating();
        }
    }

    // A run finished today, from the watch's activity history.
    private function ranToday() as Boolean {
        if (!(UserProfile has :getUserActivityHistory)) {
            return false;
        }
        var it = UserProfile.getUserActivityHistory();
        var now = Time.now().value();
        var today = Time.today().value();
        var a = it != null ? it.next() : null;
        while (a != null) {
            if (a.type == Activity.SPORT_RUNNING && a.startTime != null
                    && Partner.finishedToday(a.startTime.value(), a.duration != null ? a.duration.value() : 0, now, today)) {
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
