import Toybox.Activity;
import Toybox.ActivityMonitor;
import Toybox.Application;
import Toybox.Graphics;
import Toybox.Lang;
import Toybox.System;
import Toybox.Time;
import Toybox.Time.Gregorian;
import Toybox.Timer;
import Toybox.WatchUi;

// true: the simulator steps through all 21 Pokémon, one per minute (Monday day, night, reward,
// then Tuesday ...). Never ship it on.
const DEMO = false;

// Emerald's battle screen (layout "B, your Pokémon"), top to bottom over the field: the time;
// phone, weekday + date, notifications, battery; weather, high / low, next sun; the Pokémon on
// its platform (the reward shiny after a run) beside its battle box (name, Lv = this week's
// running km, HP = Body Battery, DEX, EXP toward the weekly km goal, a status tag, steps); the
// dialogue box with heart rate, temperature and distance or today's run, or "A wild ... appeared!"
// on the wake after the reward. Weather moves over the field while awake.
// In low power the Pokémon holds one frame; a wrist raise plays its 2-frame idle for the few
// seconds the watch stays awake. The watch's numbers are read once a minute and
// on waking (refresh); animation frames only redraw them, to spare the battery.
class PokefaceView extends WatchUi.WatchFace {
    const CX = 130;
    const FRAME_MS = 250;   // 2 frames: a 2 Hz idle, like the game's

    private var _sprite as Sprite = new Sprite();
    private var _timer as Timer.Timer or Null;
    private var _frame as Number = 0;
    private var _frames as Number = 0;    // frames played since this wake
    private var _minute as Number = -1;   // -1: read everything on the next update
    private var _species as Number = 0;
    private var _night as Boolean = false;   // decided once a minute, for the field and the Pokémon alike
    private var _runs as Array = [false, null, null, 0];   // Runs.summary: read on show / wake / new day
    private var _runDays as Array<Number> = [] as Array<Number>;
    private var _stepDays as Array<Number> = [] as Array<Number>;
    private var _runsDay as Number = -1;   // the day (00:00) the history was last read
    private var _week as Number = 0;       // this week's Monday 00:00
    private var _earned as Boolean = false;
    private var _shiny as Boolean = false;   // the reward, earned with a run today
    private var _badge as Number = Partner.BADGE_NONE;
    private var _woke as Boolean = false;    // decide the "appeared!" line at the next refresh
    private var _appear as Number = Partner.APPEAR_NONE;   // shown while this wake's animation runs
    private var _dex as Number = 0;
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
    private var _batPct as Number = 0;
    private var _field as WatchUi.BitmapResource or Null;   // today's battle field, day or night
    private var _fieldNight as Boolean = false;
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
        readRuns();
        _woke = true;
        _minute = -1;
        animate();
    }

    function onExitSleep() as Void {
        readRuns();
        _woke = true;
        _minute = -1;
        animate();
    }

    function onEnterSleep() as Void {
        stopAnimating();
        WatchUi.requestUpdate();   // back to frame 0
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
        _appear = Partner.APPEAR_NONE;   // the "appeared!" line lasts one wake
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
        }
        var night = _night;
        var ink = Battle.ink(night);
        var shade = Battle.shade(night);
        if (_field == null || night != _fieldNight) {   // only the current field is loaded
            _field = null;
            _field = WatchUi.loadResource(night ? Rez.Drawables.FieldNight : Rez.Drawables.FieldDay)
                     as WatchUi.BitmapResource;
            _fieldNight = night;
        }
        dc.drawBitmap(0, 0, _field);
        if (_timer != null) {
            Battle.weather(dc, _weather, _frame, night);   // only while awake
        }

        // The time, big
        Battle.text(dc, CX, 28, _fTime, Fmt.clock(clock.hour, clock.min, is24), ink, shade, 3,
                    Graphics.TEXT_JUSTIFY_CENTER);

        // The Pokémon on its platform
        Battle.platform(dc, 62, 182, night);
        drawPartner(dc);

        // Two small lines, over the Pokémon so it never hides them, each centred:
        //   phone dot, weekday + date | envelope, notifications | battery
        //   weather, high / low | sunrise or sunset arrow, its time
        var wd = dc.getTextWidthInPixels(_day, _f1);
        var wn = dc.getTextWidthInPixels(_notes, _f1);
        var wb = dc.getTextWidthInPixels(_bat, _f1);
        var x = CX - (11 + wd + 10 + 16 + wn + 10 + 16 + wb) / 2;
        dc.setColor(ink, Graphics.COLOR_TRANSPARENT);
        if (_phone) {
            dc.fillCircle(x + 3, 54, 3);
        } else {
            dc.drawCircle(x + 3, 54, 3);
        }
        x = small(dc, x + 11, 54, _day, ink, shade) + 10;
        Icons.envelope(dc, x + 6, 54, ink);
        x = small(dc, x + 16, 54, _notes, ink, shade) + 10;
        Icons.battery(dc, x + 7, 54, _batPct, ink);
        small(dc, x + 16, 54, _bat, ink, shade);

        var hasSun = _sunMark.length() > 0;
        var wh = dc.getTextWidthInPixels(_hiLo, _f1);
        var ws = dc.getTextWidthInPixels(_sunText, _f1);
        x = CX - (21 + wh + 10 + (hasSun ? 12 : 0) + ws) / 2;
        Icons.weather(dc, x + 8, 67, _weather);
        x = small(dc, x + 21, 67, _hiLo, ink, shade) + 10;
        if (hasSun) {
            Icons.sunArrow(dc, x + 5, 67, _sunMark.equals("↑"));
            x += 12;
        }
        small(dc, x, 67, _sunText, ink, shade);

        // Its info box: name, Lv (this week's running km), HP = Body Battery, then the Pokédex and
        // Body Battery, EXP toward the weekly km goal, a status tag and the steps
        var bx = 116;
        var by = 80;
        var bi = night ? 0xFFFFAA : 0x000000;   // text in the battle box
        var bs = night ? 0x000000 : 0xAAAAAA;
        Battle.infoBox(dc, bx, by, 130, 80, night);
        Battle.text(dc, bx + 8, by + 15, _f2, Partner.NAMES[_species] as String, bi, bs, 2, Graphics.TEXT_JUSTIFY_LEFT);
        Battle.text(dc, bx + 8, by + 34, _f1, "Lv" + _lv, bi, bs, 1, Graphics.TEXT_JUSTIFY_LEFT);
        Battle.text(dc, bx + 38, by + 34, _f1, "HP", 0xFF5500, bs, 1, Graphics.TEXT_JUSTIFY_LEFT);
        Battle.bar(dc, bx + 54, by + 32, 62, _bbFrac, hpInk(Partner.hpColour(_bbValue)));
        var dex = "DEX " + _dex + "/7";
        Battle.text(dc, bx + 8, by + 47, _f1, dex, bi, bs, 1, Graphics.TEXT_JUSTIFY_LEFT);
        Battle.text(dc, bx + 114, by + 47, _f1, "BB " + _bb, bi, bs, 1, Graphics.TEXT_JUSTIFY_RIGHT);
        Battle.text(dc, bx + 8, by + 60, _f1, "EXP", night ? 0x00AAFF : 0x0055FF, bs, 1, Graphics.TEXT_JUSTIFY_LEFT);
        Battle.bar(dc, bx + 34, by + 58, 82, _bar, 0x00AAFF);
        Battle.badge(dc, bx + 8, by + 71, _badge, _f1);   // left of the steps, where there's room
        Battle.text(dc, bx + 108, by + 71, _f1, _steps + " steps", bi, bs, 1, Graphics.TEXT_JUSTIFY_RIGHT);

        // The dialogue box: heart rate | temperature, then distance (or today's run card); on the
        // wake after the reward, "A wild / shiny ... appeared!" instead. A row too wide for the round
        // edge (big numbers) drops to 1x rather than being cut.
        Battle.dialogue(dc, 0, 192, dc.getWidth(), 70, night);
        bi = 0xFFFFFF;   // text in the dialogue box: white on navy
        bs = 0x000000;
        if (_appear != Partner.APPEAR_NONE && _timer != null) {
            appeared(dc, bi, bs);
            return;
        }
        var f = _f2;
        var px = 2;
        var whr = dc.getTextWidthInPixels(_hr, f);
        var wt = dc.getTextWidthInPixels(_temp, f);
        if (!Fit.fits(16 + whr + 20 + 14 + wt, 200, 219, 4)) {
            f = _f1;
            px = 1;
            whr = dc.getTextWidthInPixels(_hr, f);
            wt = dc.getTextWidthInPixels(_temp, f);
        }
        x = CX - (16 + whr + 20 + 14 + wt) / 2;
        Icons.heart(dc, x + 6, 210, 0xFF0055);
        Battle.text(dc, x + 16, 210, f, _hr, bi, bs, px, Graphics.TEXT_JUSTIFY_LEFT);
        x += 16 + whr + 20;
        Icons.thermometer(dc, x + 4, 210, bi);
        Battle.text(dc, x + 14, 210, f, _temp, bi, bs, px, Graphics.TEXT_JUSTIFY_LEFT);

        f = _f2;
        px = 2;
        var wdist = dc.getTextWidthInPixels(_dist, f);
        if (!Fit.fits(16 + wdist, 225, 244, 4)) {
            f = _f1;
            px = 1;
            wdist = dc.getTextWidthInPixels(_dist, f);
        }
        x = CX - (16 + wdist) / 2;
        Icons.feet(dc, x + 5, 234, bi);
        Battle.text(dc, x + 16, 234, f, _dist, bi, bs, px, Graphics.TEXT_JUSTIFY_LEFT);
    }

    // "A wild LATIAS / appeared!" on the dialogue box's two rows, 2x where it fits, else 1x.
    private function appeared(dc as Dc, ink as Number, shade as Number) as Void {
        var rows = [(_appear == Partner.APPEAR_SHINY ? "A shiny " : "A wild ") + (Partner.NAMES[_species] as String),
                    "appeared!"];
        var ys = [210, 234];
        for (var i = 0; i < 2; i++) {
            var f = _f2;
            var px = 2;
            if (!Fit.fits(dc.getTextWidthInPixels(rows[i], f), ys[i] - 10, ys[i] + 9, 4)) {
                f = _f1;
                px = 1;
            }
            Battle.text(dc, CX, ys[i], f, rows[i], ink, shade, px, Graphics.TEXT_JUSTIFY_CENTER);
        }
    }

    // A 1x piece of a small line at (x, y); returns where it ends.
    private function small(dc as Dc, x as Number, y as Number, s as String, ink as Number, shade as Number) as Number {
        Battle.text(dc, x, y, _f1, s, ink, shade, 1, Graphics.TEXT_JUSTIFY_LEFT);
        return x + dc.getTextWidthInPixels(s, _f1);
    }

    // Emerald's HP colours; unknown Body Battery leaves the bar empty.
    private function hpInk(c as Number) as Number or Null {
        return c == Partner.HP_GREEN ? 0x00FF00 : (c == Partner.HP_YELLOW ? 0xFFAA00 : (c == Partner.HP_RED ? 0xFF0000 : null));
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
        _hiLo = (cond != null ? Fmt.temp(cond.highTemperature, metric) : "--") + "/"
                + (cond != null ? Fmt.temp(cond.lowTemperature, metric) : "--");
        _weather = Icons.group(cond != null ? cond.condition : null);
        _temp = cond != null ? Fmt.temp(cond.temperature, metric) : "--";

        var now = Gregorian.info(Time.now(), Time.FORMAT_MEDIUM);
        var dow = Gregorian.info(Time.now(), Time.FORMAT_SHORT).day_of_week as Number;
        _day = Fmt.weekday(dow) + " " + now.day.toString();
        _batPct = System.getSystemStats().battery.toNumber();
        _bat = _batPct.toString() + "%";
        _notes = set.notificationCount.toString();
        _phone = set.phoneConnected;

        _hr = Fmt.num(Stats.heartRate());
        var bb = Stats.bodyBattery();
        _bb = Fmt.num(bb);
        _bbValue = bb;
        _bbFrac = bb != null ? (bb < 0 ? 0 : (bb > 100 ? 100 : bb)) / 100.0 : 0.0;
        _steps = Fmt.num(act.steps);
        var metricDist = set.distanceUnits == System.UNIT_METRIC;
        _dist = Fmt.dist(act.distance, metricDist);

        if (Time.today().value() != _runsDay) {
            readRuns();   // a new day: today's runs start over
        }
        decide(clock, act);
        var ran = _runs[0] as Boolean;
        _shiny = _earned && ran;
        _lv = Runs.level(_runs[3] as Number).toString();
        _bar = Runs.expFraction(_runs[3] as Number);
        if (ran) {
            _dist = Runs.card(_runs[1] as Number or Null, _runs[2] as Number or Null, metricDist);
        }
        _badge = Partner.badge(_batPct, _phone, _night, bb);
        if (_woke) {   // the first refresh of a wake: does the reward get its line?
            _woke = false;
            var today = Time.today().value();
            var stored = Application.Storage.getValue("appear") as Array or Null;
            var shown = stored != null && stored.size() == 2 && stored[0] == today ? stored[1] as Number : 0;
            var a = Partner.appear(shown, _earned, _shiny);
            _appear = a[0] as Number;
            if (a[1] != shown) {
                Application.Storage.setValue("appear", [today, a[1]] as Application.PropertyValueType);
            }
        }
        var today = Time.today().value();
        var days = [] as Array<Number>;   // a fresh list: addAll would grow the cached ones every minute
        days.addAll(_runDays);
        days.addAll(_stepDays);
        if (_earned) {
            days.add(today);
        }
        _dex = Partner.dexCount(days, _week, today);
    }

    // The runs (today's latest, this week's km) and the step-goal days, from the watch's history:
    // read when the face appears or wakes (that includes coming back from a run) and on a new day,
    // not every minute.
    private function readRuns() as Void {
        var now = Time.now().value();
        var today = Time.today().value();
        var dow = Gregorian.info(Time.now(), Time.FORMAT_SHORT).day_of_week as Number;
        _week = Partner.weekStart(today, dow);
        var acts = Stats.activities();
        _runs = Runs.summary(acts, now, today, _week);
        _runDays = Runs.runDays(acts, now, today, _week);
        _stepDays = Stats.stepGoalDays();
        _runsDay = today;
    }

    // Today's Pokémon, its 128 px square's top-left at (-2, 64); frame 0 unless it's animating.
    private function drawPartner(dc as Dc) as Void {
        _sprite.show(_species, _shiny);
        _sprite.draw(dc, -2, 64, _timer != null ? _frame : 0);
    }

    // Who is out: the weekday's day or night Pokémon, or its reward once a run or the step goal is done.
    private function decide(clock as System.ClockTime, act as ActivityMonitor.Info) as Void {
        var dow = Gregorian.info(Time.now(), Time.FORMAT_SHORT).day_of_week as Number;
        if (DEMO) {
            var i = clock.min % 21;
            _species = i;
            _night = i % 3 == 1;
            return;
        }
        _earned = Partner.earned(act.steps, act.stepGoal, _runs[0] as Boolean);
        _night = !Partner.isDay(Time.now().value(), clock.hour, _rise, _set);
        _species = Partner.who(dow, !_night, _earned);
    }
}
