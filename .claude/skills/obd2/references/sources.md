# Sources, and what is verified on the car

## Verified on João's City 2022 (the repo is the source)

- Supported mode 01 list, three ECUs, no 05/0F/2F/5E, 67/68 present, smart-charging idle
  voltage, clone replies > 1 s late: `docs/superpowers/archive/specs/2026-10-28-car-health-check-design.md`
  (car run 2026-10-19, old dual-mode clone). The raw hex was not kept.
- The deck's init (ATZ ATE0 ATL0 ATS0 ATH0 ATSP0) ran on that clone: `src/obd/obdlink.cpp`.
- **Not yet on the car:** the Vgate iCar Pro 2S (arrived 2026-11-17), `ATDPN`, the GATT layout,
  any Mode 22 request, any request-rate measurement.

## Web

| Id | Source | Used for |
|---|---|---|
| S1 | Wikipedia, "OBD-II PIDs" — https://en.wikipedia.org/wiki/OBD-II_PIDs | modes, mode 01/09 formulas, DTC layout |
| S2 | Elm Electronics, ELM327 datasheet (ELM327DSL) — https://web.archive.org/web/2022/https://www.elmelectronics.com/DSheets/ELM327DSL.pdf | AT commands, protocols 6/7, ATST, adaptive timing, reply count digit, multi-PID, multi-frame print format, BUFFER FULL, response pending |
| S3 | ISO 15765-4:2005 (public copy) — https://law.resource.org/pub/us/cfr/ibr/inc/iso/iso.15765-4.2005.pdf | 11/29-bit IDs, flow control 30 00 00, P2 50 ms, N_x timeouts |
| S4 | OBDb, Honda-City (+ Honda-Civic, Honda-Fit, Honda-HR-V, Honda) signal sets and test captures — https://github.com/OBDb/Honda-City | 29-bit IDs on the City, ECU addresses, Mode 22 DIDs (fuel level 7029, odometer 7022, ...) |
| S5 | meatpiHQ WiCAN vehicle profiles, Honda (`ATSP7`) — https://github.com/meatpiHQ/wican-fw/tree/main/vehicle_profiles/honda | 29-bit for Honda |
| S6 | canair VEHICLE-PROTOCOLS (Honda Clarity headers 18DA01F1 / 18DA60F1) — https://github.com/philipkocanda/canair/blob/main/profiles/VEHICLE-PROTOCOLS.md | 29-bit for Honda |
| S7 | ABRP feature board, Vgate iCar Pro BLE 4.0 UUIDs — https://abrp.featurebase.app/p/add-odb-support-for-vgate-icar-pro-bluetooth-40-ble-2 | 18F0 / 2AF0 / 2AF1 |
| S8 | Home Assistant community, ESPHome BLE ELM327 component — https://community.home-assistant.io/t/custom-esphome-component-for-ble-elm327-obd2/1011293 | the same UUID set on another adapter |
| S9 | Clarity PHEV group, ELM adapter specifics — https://clarity-phev.github.io/Battery-Capacity-Read/ELM_Adapter_Specifics.pdf | iCar Pro `ELM327 v2.2`, 30-min auto-sleep, ATPP sleep setting, multi-frame robustness |
| S10 | Torque-BHP forum, "PIDs/second" — https://torque-bhp.com/community/main-forum/pids-second-concern/ | realistic BT clone rates |
| S11 | Vgate iCar Pro reviews (iamcarhacker) — https://iamcarhacker.com/vgate-icar-pro-2s-review/ , https://iamcarhacker.com/vgate-icar-pro-ble4-0-owners-review/ | "ELM 2.3" marketing, two BLE names (iOS / Android) — LOW |
| S12 | California BAR, permanent DTCs — https://www.bar.ca.gov/arsc/newsletters/newsletter/fall-2019/permanent-diagnostic-trouble-codes | mode 0A survives clearing |
| S13 | Snap-on, OBD-II service modes — https://www.snapon.com/DiagnosticsManuals/Help/OBDII/Content/Common/Help%20Topics/OBD/OBDII%20Service%20Modes.htm | mode 04 side effects |
| S14 | SAE paper, OBD response timing — https://saemobilus.sae.org/articles/analysis-factors-impact-response-timing-obd-diagnostic-protocol-sae-pp-00314 | P2 50 ms |

## Not found (as of 2026-11-19)

The Vgate's real chip (ELM clone vs STN), the 2S's ATI string and GATT layout, its wake-up
voltage, any BLE request-rate benchmark, any capture of a Brazilian City GN 1.5 flex, and the
City's CVT temperature / gear / oil-life DIDs.
