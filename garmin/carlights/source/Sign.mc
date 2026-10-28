import Toybox.Cryptography;
import Toybox.Lang;
import Toybox.StringUtil;

// Signed requests, the same as the decks' (src/util/wire/sigreq.h): the watch never sends the key.
//   sig = the first 16 bytes of HMAC-SHA256(DECK_KEY, "GET|" + path + "|" + params + "|" + t + "|" + n), hex
// params: the request's other params sorted by name, each name=value percent-encoded (all but
// A-Z a-z 0-9 - . _ ~ as %XX, upper-case hex), joined by "&". t: the watch's clock in Unix seconds
// (synced from the phone); n: 8 random bytes in hex. The deck accepts t within 120 s of its own clock.
module Sign {
    const HEX_LOWER = "0123456789abcdef";
    const HEX_UPPER = "0123456789ABCDEF";

    // The t, n and sig params for a GET of `path` with `params` ([name, value] pairs, sorted by name).
    function auth(key as String, path as String, params as Array<Array<String> >, t as Number,
                  nonce as ByteArray) as Dictionary<String, String> {
        var ts = t.toString();
        var ns = hex(nonce, HEX_LOWER);
        var msg = "GET|" + path + "|";
        for (var i = 0; i < params.size(); i++) {
            if (i > 0) {
                msg += "&";
            }
            msg += encode(params[i][0]) + "=" + encode(params[i][1]);
        }
        msg += "|" + ts + "|" + ns;
        var mac = new Cryptography.HashBasedMessageAuthenticationCode({
            :algorithm => Cryptography.HASH_SHA256,
            :key => bytesOf(key)
        });
        mac.update(bytesOf(msg));
        return {"t" => ts, "n" => ns, "sig" => hex(mac.digest().slice(0, 16), HEX_LOWER)};
    }

    function bytesOf(s as String) as ByteArray {
        return StringUtil.convertEncodedString(s, {
            :fromRepresentation => StringUtil.REPRESENTATION_STRING_PLAIN_TEXT,
            :toRepresentation => StringUtil.REPRESENTATION_BYTE_ARRAY
        }) as ByteArray;
    }

    function hex(b as ByteArray, digits as String) as String {
        var d = digits.toCharArray();
        var s = "";
        for (var i = 0; i < b.size(); i++) {
            s += d[b[i] >> 4].toString() + d[b[i] & 15].toString();
        }
        return s;
    }

    // Percent-encodes the UTF-8 bytes of s.
    function encode(s as String) as String {
        var b = bytesOf(s);
        var up = HEX_UPPER.toCharArray();
        var out = "";
        for (var i = 0; i < b.size(); i++) {
            var c = b[i];
            if ((c >= 0x41 && c <= 0x5A) || (c >= 0x61 && c <= 0x7A) || (c >= 0x30 && c <= 0x39) || c == 0x2D ||
                c == 0x2E || c == 0x5F || c == 0x7E) {
                out += c.toChar().toString();
            } else {
                out += "%" + up[c >> 4].toString() + up[c & 15].toString();
            }
        }
        return out;
    }
}
