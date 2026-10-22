import Toybox.Communications;
import Toybox.Graphics;
import Toybox.Lang;
import Toybox.WatchUi;

// The deck on the iPhone hotspot (net/wifi pins it there).
const DECK_URL = "http://172.20.10.11/say";

class CarLightsView extends WatchUi.View {
    private var _status as String = "START: purple";

    function initialize() {
        View.initialize();
    }

    function send(text as String) as Void {
        _status = "Sending...";
        WatchUi.requestUpdate();
        Communications.makeWebRequest(DECK_URL, {"text" => text, "k" => DECK_KEY},
            {:method => Communications.HTTP_REQUEST_METHOD_GET,
             :responseType => Communications.HTTP_RESPONSE_CONTENT_TYPE_TEXT_PLAIN},
            method(:onReply));
    }

    // code: the HTTP status, or a negative Communications error (e.g. -300 timeout,
    // -1001 "secure connection required" if iOS blocks plain http).
    function onReply(code as Number, data as Dictionary or String or Null) as Void {
        _status = code == 200 && data != null ? data.toString() : "Error " + code;
        System.println("deck reply: " + code + " " + data);
        WatchUi.requestUpdate();
    }

    function onUpdate(dc as Dc) as Void {
        dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_BLACK);
        dc.clear();
        dc.drawText(dc.getWidth() / 2, dc.getHeight() / 2 - 30, Graphics.FONT_SMALL, "Car lights",
                    Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        dc.setColor(Graphics.COLOR_PURPLE, Graphics.COLOR_TRANSPARENT);
        dc.drawText(dc.getWidth() / 2, dc.getHeight() / 2 + 10, Graphics.FONT_TINY, _status,
                    Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
    }
}
