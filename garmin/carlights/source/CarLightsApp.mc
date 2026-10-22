// Car lights, step 1: a one-button test. START sends "purple" to the old deck's LED panel
// (GET http://172.20.10.11/say, the Siri shortcut's endpoint) through Garmin Connect on the
// iPhone, and shows what came back. It answers whether Garmin on iOS lets a watch reach a plain
// http:// address on the phone's own hotspot.
import Toybox.Application;
import Toybox.Lang;
import Toybox.WatchUi;

class CarLightsApp extends Application.AppBase {
    function initialize() {
        AppBase.initialize();
    }

    function getInitialView() as [WatchUi.Views] or [WatchUi.Views, WatchUi.InputDelegates] {
        var view = new CarLightsView();
        return [view, new CarLightsDelegate(view)];
    }
}
