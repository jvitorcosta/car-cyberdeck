import Toybox.Activity;
import Toybox.ActivityMonitor;
import Toybox.Graphics;
import Toybox.Lang;
import Toybox.System;
import Toybox.Time;
import Toybox.Time.Gregorian;
import Toybox.Timer;
import Toybox.UserProfile;
import Toybox.WatchUi;

// true: the simulator steps through all 21 Pokémon, one per minute (Monday day, night, reward,
// then Tuesday ...). Never ship it on.
const DEMO = false;

// Layout "A, all at once", top to bottom: weather + next sunrise/sunset; weekday, date, phone,
// notifications, battery; the time; the partner with heart rate + temperature on its left and
// Body Battery + calories on its right; the step bar; steps and distance.
// In low power the Pokémon holds one frame; a wrist raise plays its 2-frame idle for the few
// seconds the watch stays awake. The watch's numbers are read once a minute and
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
    private var _species as Number = 0;
    private var _rise as Number or Null;   // today's sunrise / sunset, epoch seconds
    private var _set as Number or Null;

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
    private var _kcal as String = "--";
    private var _steps as String = "--";
    private var _dist as String = "--";
    private var _lv as String = "";
    private var _bbValue as Number or Null;
    private var _bbFrac as Float = 0.0;
    private var _bar as Float = 0.0;
    private var _f1 as Graphics.FontType;   // Emerald's font at 1x, 2x, and 3x digits for the time
    private var _f2 as Graphics.FontType;
    private var _fTime as Graphics.FontType;

    function initialize() {
        WatchFace.initialize();
        _f1 = WatchUi.loadResource(Rez.Fonts.Emerald1) as Graphics.FontType;
        _f2 = WatchUi.loadResource(Rez.Fonts.Emerald2) as Graphics.FontType;
        _fTime = WatchUi.loadResource(Rez.Fonts.EmeraldTime) as Graphics.FontType;
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
        if (_timer != null) {
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
        var night = DEMO ? (clock.min % 21) % 3 == 1 : !Partner.isDay(Time.now().value(), clock.hour, _rise, _set);
        var ink = night ? 0xFFFFFF : 0x555555;       // text on the field
        var shade = night ? 0x555555 : 0xAAAAAA;     // its shadow
        Battle.field(dc, night);

        // The time, big
        Battle.text(dc, CX, 28, _fTime, Fmt.clock(clock.hour, clock.min, is24), ink, shade, 3,
                    Graphics.TEXT_JUSTIFY_CENTER);

        // One small line: weekday + date, phone, notifications, battery, weather, high / low, next sun
        var hasSun = _sunMark.length() > 0;
        var parts = [_day, _notes, _bat, _hiLo, _sunText];
        var w = 12 + 12 + 6 + 16 + (hasSun ? 12 : 0) + 5 * 6;   // icons and gaps
        for (var i = 0; i < parts.size(); i++) {
            w += dc.getTextWidthInPixels(parts[i] as String, _f1);
        }
        var x = CX - w / 2;
        x = small(dc, x, _day, ink, shade) + 6;
        dc.setColor(0x0055FF, Graphics.COLOR_TRANSPARENT);
        if (_phone) {
            dc.fillCircle(x + 3, 58, 3);
        } else {
            dc.drawCircle(x + 3, 58, 3);
        }
        x += 12;
        Icons.envelope(dc, x + 6, 58, ink);
        x = small(dc, x + 14, _notes, ink, shade) + 6;
        x = small(dc, x, _bat, ink, shade) + 6;
        Icons.weather(dc, x + 8, 58, _weather);
        x = small(dc, x + 18, _hiLo, ink, shade) + 6;
        if (hasSun) {
            Icons.sunArrow(dc, x + 6, 58, _sunMark.equals("↑"));
            x += 12;
        }
        small(dc, x, _sunText, ink, shade);

        // The Pokémon on its platform
        Battle.platform(dc, 62, 182, night);
        drawPartnerAt(dc, CX, SPRITE_Y);

        // Its info box: name, Lv (day of the month), HP = Body Battery, EXP = steps
        var bx = 116;
        var by = 80;
        Battle.infoBox(dc, bx, by, 130, 80);
        Battle.text(dc, bx + 8, by + 15, _f2, Partner.NAMES[_species] as String, 0x555555, 0xAAAAAA, 2,
                    Graphics.TEXT_JUSTIFY_LEFT);
        Battle.text(dc, bx + 8, by + 34, _f1, "Lv" + _lv, 0x555555, 0xAAAAAA, 1, Graphics.TEXT_JUSTIFY_LEFT);
        Battle.text(dc, bx + 38, by + 34, _f1, "HP", 0xFFAA00, 0x555555, 1, Graphics.TEXT_JUSTIFY_LEFT);
        Battle.bar(dc, bx + 54, by + 32, 66, _bbFrac, hpInk(Partner.hpColour(_bbValue)));
        Battle.text(dc, bx + 120, by + 47, _f1, "BB " + _bb, 0x555555, 0xAAAAAA, 1, Graphics.TEXT_JUSTIFY_RIGHT);
        Battle.text(dc, bx + 8, by + 60, _f1, "EXP", 0x0055FF, 0xAAAAAA, 1, Graphics.TEXT_JUSTIFY_LEFT);
        Battle.bar(dc, bx + 34, by + 58, 86, _bar, 0x55AAFF);
        Battle.text(dc, bx + 120, by + 71, _f1, _steps, 0x555555, 0xAAAAAA, 1, Graphics.TEXT_JUSTIFY_RIGHT);

        // The battle menu: heart rate, temperature / calories, distance
        Battle.menuBox(dc, 0, 192, dc.getWidth(), 70);
        var whr = dc.getTextWidthInPixels(_hr, _f2);
        var wt = dc.getTextWidthInPixels(_temp, _f2);
        var x1 = CX - (16 + whr + 18 + wt) / 2;
        Icons.heart(dc, x1 + 6, 211, 0xFF5555);
        Battle.text(dc, x1 + 16, 211, _f2, _hr, 0xFFFFFF, 0x555555, 2, Graphics.TEXT_JUSTIFY_LEFT);
        Battle.text(dc, x1 + 16 + whr + 18, 211, _f2, _temp, 0xFFFFFF, 0x555555, 2, Graphics.TEXT_JUSTIFY_LEFT);
        var wk = dc.getTextWidthInPixels(_kcal, _f2);
        var wu = dc.getTextWidthInPixels("kcal", _f1);
        var wd = dc.getTextWidthInPixels(_dist, _f2);
        var x2 = CX - (wk + 2 + wu + 12 + wd) / 2;
        Battle.text(dc, x2, 233, _f2, _kcal, 0xFFFFFF, 0x555555, 2, Graphics.TEXT_JUSTIFY_LEFT);
        Battle.text(dc, x2 + wk + 2, 237, _f1, "kcal", 0xFFFFFF, 0x555555, 1, Graphics.TEXT_JUSTIFY_LEFT);
        Battle.text(dc, x2 + wk + 2 + wu + 12, 233, _f2, _dist, 0xFFFFFF, 0x555555, 2, Graphics.TEXT_JUSTIFY_LEFT);
    }

    // A 1x piece of the small line at x; returns where it ends.
    private function small(dc as Dc, x as Number, s as String, ink as Number, shade as Number) as Number {
        Battle.text(dc, x, 58, _f1, s, ink, shade, 1, Graphics.TEXT_JUSTIFY_LEFT);
        return x + dc.getTextWidthInPixels(s, _f1);
    }

    // Emerald's HP colours; unknown Body Battery leaves the bar empty.
    private function hpInk(c as Number) as Number or Null {
        return c == Partner.HP_GREEN ? 0x55FFAA : (c == Partner.HP_YELLOW ? 0xFFFF55 : (c == Partner.HP_RED ? 0xFF5555 : null));
    }

    // Reads the watch's numbers and decides who is out; once a minute and on waking.
    private function refresh(clock as System.ClockTime, is24 as Boolean) as Void {
        var set = System.getDeviceSettings();
        var metric = set.temperatureUnits == System.UNIT_METRIC;
        var act = Stats.today();
        var cond = Stats.weather();

        var sun = Stats.sun(cond);
        _rise = sun[0];
        _set = sun[1];
        var next = Fmt.nextSun(Time.now().value(), sun[0], sun[1], sun[2], is24);
        var mark = next.substring(0, 1);
        _sunMark = mark.equals("↑") || mark.equals("↓") ? mark : "";
        _sunText = _sunMark.length() > 0 ? next.substring(1, null) : next;
        _hiLo = (cond != null ? Fmt.temp(cond.highTemperature, metric) : "--") + " "
                + (cond != null ? Fmt.temp(cond.lowTemperature, metric) : "--");
        _weather = Icons.group(cond != null ? cond.condition : null);
        _temp = cond != null ? Fmt.temp(cond.temperature, metric) : "--";

        var now = Gregorian.info(Time.now(), Time.FORMAT_MEDIUM);
        _day = (now.day_of_week as String).toUpper() + " " + now.day.toString();
        _lv = now.day.toString();
        _bat = System.getSystemStats().battery.toNumber().toString() + "%";
        _notes = set.notificationCount.toString();
        _phone = set.phoneConnected;

        _hr = Fmt.num(Stats.heartRate());
        var bb = Stats.bodyBattery();
        _bb = Fmt.num(bb);
        _bbValue = bb;
        _bbFrac = bb != null ? (bb < 0 ? 0 : (bb > 100 ? 100 : bb)) / 100.0 : 0.0;
        _kcal = Fmt.num(act.calories);
        _steps = Fmt.num(act.steps);
        _dist = Fmt.dist(act.distance, set.distanceUnits == System.UNIT_METRIC);
        _bar = Partner.barFraction(act.steps, act.stepGoal);

        decide(clock, act);
    }

    function drawPartnerAt(dc as Dc, cx as Number, cy as Number) as Void {
        _sprite.show(_species);
        _sprite.draw(dc, -2, 64, _timer != null ? _frame : 0);
    }

    // Who is out: the weekday's day or night Pokémon, or its reward once a run or the step goal is done.
    private function decide(clock as System.ClockTime, act as ActivityMonitor.Info) as Void {
        var dow = Gregorian.info(Time.now(), Time.FORMAT_SHORT).day_of_week as Number;
        if (DEMO) {
            var i = clock.min % 21;
            _species = i;
            return;
        }
        var earned = Partner.earned(act.steps, act.stepGoal, false) || ranToday();
        var day = Partner.isDay(Time.now().value(), clock.hour, _rise, _set);
        _species = Partner.who(dow, day, earned);
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
}
