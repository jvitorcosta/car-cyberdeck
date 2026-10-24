import Toybox.Graphics;
import Toybox.Lang;
import Toybox.Math;
import Toybox.Weather;

// Small icons drawn with shapes, since the watch fonts have no heart, envelope or weather glyphs.
module Icons {
    enum { NONE, SUN, CLOUD, RAIN, SNOW, STORM, FOG, SAND }

    // Garmin's 50-odd weather conditions, in the six pictures the face has room for.
    function group(cond as Number or Null) as Number {
        if (cond == null) {
            return NONE;
        }
        switch (cond) {
            case Weather.CONDITION_CLEAR:
            case Weather.CONDITION_MOSTLY_CLEAR:
            case Weather.CONDITION_FAIR:
            case Weather.CONDITION_PARTLY_CLEAR:
                return SUN;
            case Weather.CONDITION_RAIN:
            case Weather.CONDITION_SCATTERED_SHOWERS:
            case Weather.CONDITION_LIGHT_RAIN:
            case Weather.CONDITION_HEAVY_RAIN:
            case Weather.CONDITION_LIGHT_SHOWERS:
            case Weather.CONDITION_SHOWERS:
            case Weather.CONDITION_HEAVY_SHOWERS:
            case Weather.CONDITION_CHANCE_OF_SHOWERS:
            case Weather.CONDITION_DRIZZLE:
            case Weather.CONDITION_FREEZING_RAIN:
            case Weather.CONDITION_UNKNOWN_PRECIPITATION:
                return RAIN;
            case Weather.CONDITION_THUNDERSTORMS:
            case Weather.CONDITION_SCATTERED_THUNDERSTORMS:
            case Weather.CONDITION_CHANCE_OF_THUNDERSTORMS:
            case Weather.CONDITION_TROPICAL_STORM:
            case Weather.CONDITION_HURRICANE:
            case Weather.CONDITION_TORNADO:
                return STORM;
            case Weather.CONDITION_SNOW:
            case Weather.CONDITION_WINTRY_MIX:
            case Weather.CONDITION_HAIL:
            case Weather.CONDITION_LIGHT_SNOW:
            case Weather.CONDITION_HEAVY_SNOW:
            case Weather.CONDITION_LIGHT_RAIN_SNOW:
            case Weather.CONDITION_HEAVY_RAIN_SNOW:
            case Weather.CONDITION_RAIN_SNOW:
            case Weather.CONDITION_CHANCE_OF_SNOW:
            case Weather.CONDITION_CHANCE_OF_RAIN_SNOW:
            case Weather.CONDITION_FLURRIES:
            case Weather.CONDITION_SLEET:
            case Weather.CONDITION_ICE_SNOW:
            case Weather.CONDITION_ICE:
                return SNOW;
            case Weather.CONDITION_FOG:
            case Weather.CONDITION_HAZY:
            case Weather.CONDITION_MIST:
            case Weather.CONDITION_SMOKE:
            case Weather.CONDITION_VOLCANIC_ASH:
                return FOG;
            case Weather.CONDITION_DUST:
            case Weather.CONDITION_HAZE:
            case Weather.CONDITION_SAND:
            case Weather.CONDITION_SANDSTORM:
                return SAND;
        }
        return CLOUD;
    }

    // A weather picture about 16 px wide, centred on (x, y).
    function weather(dc as Dc, x as Number, y as Number, kind as Number) as Void {
        if (kind == NONE) {
            return;
        }
        if (kind == SUN) {
            dc.setColor(0xFFAA00, Graphics.COLOR_TRANSPARENT);
            dc.fillCircle(x, y, 5);
            for (var i = 0; i < 8; i++) {
                var c = Math.cos(i * Math.PI / 4);
                var s = Math.sin(i * Math.PI / 4);
                dc.drawLine(x + (7 * c).toNumber(), y + (7 * s).toNumber(), x + (9 * c).toNumber(), y + (9 * s).toNumber());
            }
            return;
        }
        if (kind == FOG || kind == SAND) {
            dc.setColor(kind == SAND ? 0xFFAA55 : Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
            for (var i = -4; i <= 4; i += 4) {
                dc.drawLine(x - 7, y + i, x + 7, y + i);
            }
            return;
        }
        dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);   // the cloud
        dc.fillCircle(x - 4, y - 1, 4);
        dc.fillCircle(x + 2, y - 3, 5);
        dc.fillRectangle(x - 8, y - 1, 15, 4);
        if (kind == RAIN) {
            dc.setColor(0x00AAFF, Graphics.COLOR_TRANSPARENT);
            for (var i = -5; i <= 5; i += 5) {
                dc.drawLine(x + i, y + 5, x + i - 2, y + 9);
            }
        } else if (kind == SNOW) {
            dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
            for (var i = -5; i <= 5; i += 5) {
                dc.fillRectangle(x + i - 1, y + 6, 2, 2);
            }
        } else if (kind == STORM) {
            dc.setColor(Graphics.COLOR_YELLOW, Graphics.COLOR_TRANSPARENT);
            dc.fillPolygon([[x, y + 3], [x - 3, y + 8], [x, y + 7], [x - 2, y + 11], [x + 3, y + 6], [x, y + 6]]);
        }
    }

    function heart(dc as Dc, x as Number, y as Number, color as Number) as Void {
        dc.setColor(color, Graphics.COLOR_TRANSPARENT);
        dc.fillCircle(x - 3, y - 2, 3);
        dc.fillCircle(x + 3, y - 2, 3);
        dc.fillPolygon([[x - 6, y - 1], [x + 6, y - 1], [x, y + 6]]);
    }

    function envelope(dc as Dc, x as Number, y as Number, ink as Number) as Void {
        dc.setColor(ink, Graphics.COLOR_TRANSPARENT);
        dc.drawRectangle(x - 6, y - 4, 13, 9);
        dc.drawLine(x - 6, y - 4, x, y + 1);
        dc.drawLine(x + 6, y - 4, x, y + 1);
    }

    // A battery 13x7 centred on (x, y), filled to pct (red at 20 % or less).
    function battery(dc as Dc, x as Number, y as Number, pct as Number, ink as Number) as Void {
        dc.setColor(ink, Graphics.COLOR_TRANSPARENT);
        dc.drawRectangle(x - 7, y - 3, 12, 7);
        dc.fillRectangle(x + 5, y - 1, 2, 3);
        var fill = 8 * (pct < 0 ? 0 : (pct > 100 ? 100 : pct)) / 100;
        if (fill > 0) {
            dc.setColor(pct <= 20 ? 0xFF5555 : 0x55AA55, Graphics.COLOR_TRANSPARENT);
            dc.fillRectangle(x - 5, y - 1, fill, 3);
        }
    }

    // A thermometer about 6x14, centred on (x, y).
    function thermometer(dc as Dc, x as Number, y as Number, ink as Number) as Void {
        dc.setColor(ink, Graphics.COLOR_TRANSPARENT);
        dc.drawRoundedRectangle(x - 2, y - 7, 5, 11, 2);
        dc.setColor(0xFF5555, Graphics.COLOR_TRANSPARENT);
        dc.fillCircle(x, y + 4, 3);
        dc.fillRectangle(x - 1, y - 2, 2, 5);
    }

    // Two footprints about 12x14, centred on (x, y): distance walked.
    function feet(dc as Dc, x as Number, y as Number, ink as Number) as Void {
        dc.setColor(ink, Graphics.COLOR_TRANSPARENT);
        dc.fillEllipse(x - 3, y + 2, 2, 4);
        dc.fillEllipse(x + 3, y - 3, 2, 4);
        dc.fillCircle(x - 3, y + 7, 1);
        dc.fillCircle(x + 3, y + 2, 1);
    }

    // Sunrise (up) or sunset (down): a small arrow over a horizon line.
    function sunArrow(dc as Dc, x as Number, y as Number, up as Boolean) as Void {
        dc.setColor(0xFFAA00, Graphics.COLOR_TRANSPARENT);
        dc.drawLine(x - 5, y + 5, x + 6, y + 5);
        if (up) {
            dc.fillPolygon([[x, y - 5], [x - 4, y], [x + 4, y]]);
            dc.fillRectangle(x - 1, y, 3, 4);
        } else {
            dc.fillPolygon([[x, y + 3], [x - 4, y - 2], [x + 4, y - 2]]);
            dc.fillRectangle(x - 1, y - 6, 3, 4);
        }
    }
}
