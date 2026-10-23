// Pokéface: a Forerunner 255 watch face. The Charmander line keeps you company through the day
// (Partner picks who), among the weather, health and phone data (Stats reads it, Fmt writes it).
import Toybox.Application;
import Toybox.Lang;
import Toybox.WatchUi;

class PokefaceApp extends Application.AppBase {
    function initialize() {
        AppBase.initialize();
    }

    function getInitialView() as [WatchUi.Views] or [WatchUi.Views, WatchUi.InputDelegates] {
        return [new PokefaceView()];
    }
}
