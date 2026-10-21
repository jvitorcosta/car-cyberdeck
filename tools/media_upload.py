# PlatformIO script: the S3's `uploadmedia` target. Packs data/ (tools/pack_media.py) and writes
# the pack to the media partition over USB; the firmware is not rebuilt or flashed.
#   pio run -e s3 -t uploadmedia [--upload-port COM12]
# Only needed when a clip, the montage, a tune or the font changes.
Import("env")  # noqa: F821 (provided by PlatformIO/SCons)
import os
import sys

root = env.subst("$PROJECT_DIR")  # noqa: F821
sys.path.insert(0, os.path.join(root, "tools"))
import pack_media  # noqa: E402

out = os.path.join(env.subst("$BUILD_DIR"), "media.bin")  # noqa: F821
offset, _ = pack_media.partition()
esptool = os.path.join(env.PioPlatform().get_package_dir("tool-esptoolpy") or "", "esptool.py")  # noqa: F821

env.AddCustomTarget(  # noqa: F821
    name="uploadmedia",
    dependencies=None,
    actions=[
        '"$PYTHONEXE" "%s" "%s"' % (os.path.join(root, "tools", "pack_media.py"), out),
        env.VerboseAction(env.AutodetectUploadPort, "Looking for upload port..."),  # noqa: F821
        '"$PYTHONEXE" "%s" --chip $BOARD_MCU --port "$UPLOAD_PORT" --baud $UPLOAD_SPEED '
        '--before default_reset --after hard_reset write_flash -z 0x%X "%s"' % (esptool, offset, out),
    ],
    title="Upload media",
    description="Pack data/ and flash it to the S3's media partition",
)
