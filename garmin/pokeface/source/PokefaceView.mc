import Toybox.Graphics;
import Toybox.Lang;
import Toybox.System;
import Toybox.Time;
import Toybox.Time.Gregorian;
import Toybox.WatchUi;

// Layout "A, all at once", top to bottom: weather + next sunrise/sunset; weekday, date, phone,
// notifications, battery; the time; the partner with heart rate + temperature on its left and
// Body Battery + calories on its right; the step bar; steps and distance.
class PokefaceView extends WatchUi.WatchFace {
    const CX = 130;
    const SPRITE_Y = 150;

    function initialize() {
        WatchFace.initialize();
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
        drawPartnerAt(dc, CX, SPRITE_Y);
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

    // Stand-in until the sprites are wired in (Task 4).
    function drawPartnerAt(dc as Dc, cx as Number, cy as Number) as Void {
        dc.setColor(0xFF5500, Graphics.COLOR_TRANSPARENT);
        dc.fillRectangle(cx - 24, cy - 24, 48, 48);
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
