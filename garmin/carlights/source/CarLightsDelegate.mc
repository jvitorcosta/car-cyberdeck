import Toybox.Lang;
import Toybox.WatchUi;

class CarLightsDelegate extends WatchUi.BehaviorDelegate {
    private var _view as CarLightsView;

    function initialize(view as CarLightsView) {
        BehaviorDelegate.initialize();
        _view = view;
    }

    // START (top right) on the Forerunner 255.
    function onSelect() as Boolean {
        _view.send("purple");
        return true;
    }
}
