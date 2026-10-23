// Pokéface: a Forerunner 255 watch face drawn as Emerald's battle screen. Partner picks today's
// Pokémon, Stats reads the watch's numbers, Fmt writes them, Battle and Icons draw the scene.
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
