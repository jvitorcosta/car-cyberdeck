import Toybox.Lang;
import Toybox.Test;

// Known answers shared with the decks' host test (test/test_sigreq, from Python's hmac): the watch's
// signatures must be the decks'. Built only with monkeyc --unit-test; run in the simulator (monkeydo -t).
(:test)
function signKnownAnswers(logger as Logger) as Boolean {
    var nonce = [0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef]b;
    var a = Sign.auth("test-key-123", "/music", [["on", "1"]], 1761600000, nonce);
    Test.assertEqual(a["t"], "1761600000");
    Test.assertEqual(a["n"], "0123456789abcdef");
    Test.assertEqual(a["sig"], "a56c573a195e878e743c20d3ae4d8ef6");
    var b = Sign.auth("test-key-123", "/say", [["text", "light blue"], ["zone", "inside"]], 1761600000, nonce);
    Test.assertEqual(b["sig"], "d19dcaa52a222e8b367573882f0c4aa5");
    var c = Sign.auth("test-key-123", "/state", [], 1761600000, [0, 0, 0, 0, 0, 0, 0, 0xff]b);
    Test.assertEqual(c["sig"], "3b9b87c49ef5a6f7874aa0cd3248882f");
    return true;
}

(:test)
function signEncodesLikeTheDecks(logger as Logger) as Boolean {
    Test.assertEqual(Sign.encode("light blue"), "light%20blue");
    Test.assertEqual(Sign.encode("50%"), "50%25");
    Test.assertEqual(Sign.encode("a&b=c|d"), "a%26b%3Dc%7Cd");
    Test.assertEqual(Sign.encode("Az09-._~"), "Az09-._~");
    return true;
}
