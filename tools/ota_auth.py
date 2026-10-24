# PlatformIO post script for the OTA envs (s3_ota, ledpanel_ota): espota's password is the deck's
# REMOTE_TOKEN, read from src/config.h (git-ignored), so it never sits in platformio.ini.
Import("env")  # noqa: F821 (provided by PlatformIO/SCons)
import os
import re
import sys

config = os.path.join(env.subst("$PROJECT_DIR"), "src", "config.h")  # noqa: F821
token = ""
if os.path.isfile(config):
    m = re.search(r'^\s*#define\s+REMOTE_TOKEN\s+"([^"]*)"', open(config, encoding="utf-8").read(), re.M)
    token = m.group(1) if m else ""
if not token and "upload" in COMMAND_LINE_TARGETS:  # noqa: F821 (a plain build needs none)
    sys.stderr.write("ota_auth: no REMOTE_TOKEN in src/config.h; the deck's OTA is off without it\n")
    env.Exit(1)  # noqa: F821
env.Append(UPLOADERFLAGS=["--auth=" + token])  # noqa: F821
