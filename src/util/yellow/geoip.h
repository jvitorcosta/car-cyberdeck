#pragma once
// Roughly where the deck is, from the public IP it reaches the internet with (the phone's hotspot):
// ip-api.com's free JSON answer (plain http, no key). Good to a city, enough for the weather.
// PURE, host-tested.
namespace geoip {
constexpr const char* URL = "http://ip-api.com/json/?fields=status,lat,lon,city";
// lat / lon as the text ip-api sent, cut to 4 decimals (~10 m); city cut to 23 bytes, whole
// UTF-8 characters ("" when absent).
struct Place { char lat[12]; char lon[12]; char city[24]; };
// False unless "status":"success" and both lat and lon are numbers.
bool parse(const char* body, Place* out);
}
