"""Build data/montage.bin: the boot scene's photo montage from the owner's photos
(docs/superpowers/specs/2026-10-09-boot-scene-v3-design.md, section 4): ~5 s of photos, then the
end card, the POWERED BY HONDA logo (spec 2026-10-19-random-intro: every boot opening ends with it).
Run: python tools/gen_montage.py [photo dir]      (default: montage-photos/)
     python tools/gen_montage.py montage-photos/cartoon2d --cartoon   (2D cartoon stills, see CARTOON_SHOTS)
The photos and data/montage.bin are git-ignored (they show the number plate). Needs ffmpeg.

Format (little-endian): "MTG1", u16 count, u16 fps, count x (u32 offset, u32 size), then the
baseline JPEG frames (320x240).
"""
import glob
import os
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "data", "montage.bin")
FPS, XFADE = 10, 3                         # 10 fps, 0.3 s crossfades
CX, CY = 320, 250                          # where each car's centre lands on the 640x480 canvas
GRADE = "eq=contrast=1.06:saturation=1.08:gamma=0.97"
# file, car centre (x, y) and width in a 640-wide copy, target width, frames, push-in per frame, slide
SHOTS = [("07-right-wide-turning.jpg", 325, 603, 610, 0.95, 14, 0.0019, 0),
         ("02-front-close.jpg", 282, 655, 556, 0.87, 11, 0.005, -1),
         ("04-left-closer.jpg", 291, 653, 570, 0.89, 12, 0.005, -1)]
DETAIL = ("06-headlight-detail.jpg", 290, 18, 0.0045)       # plain crop at y, frames, push-in
# Optional moving shots: a clip in <photo dir>/clips/ named after a photo (07-right-wide-turning.mp4)
# replaces that still, from start (s) for frames at FPS, framed like the photo (same 9:16 picture,
# same car position), with the same total push-in. The detail's entry also sets its crop y.
CLIPS = {"07-right-wide-turning": (0.6, 22), "02-front-close": (0.4, 20),
         "04-left-closer": (0.2, 20), "06-headlight-detail": (2.0, 28, 200)}
# Optional wink: a clip frame with one eye closed, clips/<stem>-wink.png (same size as the clip). The shot
# then plays the clip's first open-eye frames, holds the last one, shows the wink, and holds open again.
WINK = {"06-headlight-detail": (8, 4, 5)}                  # frames: from the clip, held, winking
# Optional end card: this logo (git-ignored with the photos) centred on white with a thin red frame.
# The last photo fades to white, the logo fades in, holds, then everything fades to black.
END_LOGO = "honda-logo.png"
END_LOGO_W, END_BORDER, END_RED = 296, 4, "0xE2001A"
END_IN, END_HOLD, END_OUT = 4, 16, 5                        # frames (the held frames share one JPEG)
END_Q = 2      # q 8 left grey specks around the black letters on white (photos hide them)
# A red caption under the logo; "" = none (the owner's logo art already reads "POWERED by HONDA").
END_TEXT, END_TEXT_SIZE = "", 15
END_FONT = "C\\:/Windows/Fonts/arialbd.ttf"                 # ffmpeg drawtext path (Windows)


def ffmpeg(args):
    subprocess.run(["ffmpeg", "-loglevel", "error", "-y"] + args, check=True)


def shot_filter(cx, cy, w, target, n, rate, slide, first, clip=False):
    """A still held for n frames, or (clip) n frames of a clip, d=1 per frame."""
    s = max(1.0, target * 640 / w)             # never shrink: no margins to fill
    fw, fx, fy = 640 * s, CX - cx * s, CY - cy * s
    fx = min(0.0, max(640 - fw, fx))           # the photo always covers the full width
    x = "(iw-iw/zoom)*(0.5+0.45*(%d)*(on/%d-0.5))" % (slide, n)
    fade = ",fade=t=in:st=0:d=0.8" if first else ""
    return ("%sscale=640:-1,scale=iw*%.4f:-1,crop=640:480:%d:%d,%s,"
            "zoompan=z='1.0+%s*on':x='%s':y='(ih-ih/zoom)/2':d=%d:s=320x240:fps=%d,vignette=PI/8%s,format=yuv420p"
            % ("fps=%d," % FPS if clip else "", s, round(-fx), round(-fy), GRADE, rate, x,
               1 if clip else n, FPS, fade))


def wink_sequence(frames, opened, hold, wink):
    """The wink shot's frames: clip frame indexes, -1 for the wink frame."""
    seq = list(range(opened)) + [opened - 1] * hold + [-1] * wink
    return seq + [opened - 1] * (frames - len(seq))


def wink_frame(photos, stem):
    """The shot's wink frame, or None."""
    path = os.path.join(photos, "clips", stem + "-wink.png")
    return path if stem in WINK and os.path.isfile(path) else None


def wink_clip(clip, start, frames, wink, stem, tmp):
    """The clip's shot with the wink in it, as a clip of its own at FPS."""
    d = os.path.join(tmp, "wink")
    os.makedirs(d)
    opened = WINK[stem][0]
    ffmpeg(["-ss", "%.2f" % start, "-i", clip, "-vf", "fps=%d" % FPS, "-frames:v", str(opened),
            "-start_number", "0", os.path.join(d, "c%03d.png")])
    for i, k in enumerate(wink_sequence(frames, *WINK[stem])):
        shutil.copy(wink if k < 0 else os.path.join(d, "c%03d.png" % k), os.path.join(d, "s%03d.png" % i))
    out = os.path.join(tmp, stem + "-wink.mp4")
    ffmpeg(["-framerate", str(FPS), "-i", os.path.join(d, "s%03d.png"), "-c:v", "libx264", "-crf", "12",
            "-pix_fmt", "yuv420p", out])
    return out


def source(photos, name, n, rate, tmp=None):
    """The ffmpeg input for a shot, its frames and push-in per frame: the photo, or its clip."""
    stem = os.path.splitext(name)[0]
    clip = os.path.join(photos, "clips", stem + ".mp4")
    if stem in CLIPS and os.path.isfile(clip):
        start, frames = CLIPS[stem][:2]
        wink = wink_frame(photos, stem)
        if wink and tmp:
            return ["-i", wink_clip(clip, start, frames, wink, stem, tmp)], frames, rate * n / frames, True
        return ["-ss", "%.2f" % start, "-i", clip], frames, rate * n / frames, True
    src = os.path.join(photos, name)
    if not os.path.isfile(src):
        sys.exit("gen_montage: missing photo %s" % src)
    return ["-i", src], n, rate, False


def photo_frames():
    """Frames of the photo part (stills): every shot and the detail, minus the crossfade overlaps."""
    n = [s[5] for s in SHOTS] + [DETAIL[2]]
    return sum(n) - XFADE * (len(n) - 1)


def end_filter(cw, ch, x0, y0, w=1, h=1, text=None):
    """The end card: the logo (w x h source, cropped cw x ch at x0, y0) on white, the red frame,
    with a caption (text, default END_TEXT) in the frame's red the logo is raised for it, the fades."""
    text = END_TEXT if text is None else text
    flat = "color=white:s=%dx%d[w];[w][0]overlay=shortest=1" % (w, h)
    caption = ("drawtext=fontfile='%s':text='%s':fontsize=%d:fontcolor=%s:x=(w-text_w)/2:y=h-46,"
               % (END_FONT, text, END_TEXT_SIZE, END_RED)) if text else ""
    return (flat + ",crop=%d:%d:%d:%d,scale=%d:-1:flags=area[l];[1][l]overlay=(W-w)/2:(H-h)/2-%d,"
            "drawbox=x=0:y=0:w=320:h=240:color=%s:t=%d,%s"
            "fade=t=in:s=0:n=%d:color=white,fade=t=out:s=%d:n=%d,format=yuvj420p"
            % (cw, ch, x0, y0, END_LOGO_W, 16 if text else 6, END_RED, END_BORDER, caption,
               END_IN, END_IN + END_HOLD, END_OUT - 1))


def end_card(photos, tmp):
    """JPEG frames of the end card, or [] without a logo. Encoded straight from stills, so the
    held frames come out byte-identical and pack() stores them once."""
    src = os.path.join(photos, END_LOGO)
    if not os.path.isfile(src):
        return []
    w, h = (int(v) for v in subprocess.run(
        ["ffprobe", "-v", "error", "-show_entries", "stream=width,height", "-of", "csv=p=0", src],
        check=True, capture_output=True, text=True).stdout.split(",")[:2])
    flat = "color=white:s=%dx%d[w];[w][0]overlay=shortest=1" % (w, h)    # transparency on white
    gray = os.path.join(tmp, "logo.gray")
    ffmpeg(["-i", src, "-filter_complex", flat + ",format=gray", "-frames:v", "1", "-f", "rawvideo", gray])
    px = open(gray, "rb").read()
    rows = [y for y in range(h) if min(px[y * w:(y + 1) * w]) < 128]
    cols = [x for x in range(w) if min(px[x::w]) < 128]
    if not rows:
        return []
    x0, y0 = cols[0], rows[0]
    n = END_IN + END_HOLD + END_OUT
    ffmpeg(["-i", src, "-f", "lavfi", "-i", "color=white:s=320x240:r=%d" % FPS, "-filter_complex",
            end_filter(cols[-1] - x0 + 1, rows[-1] - y0 + 1, x0, y0, w, h),
            "-frames:v", str(n), "-q:v", str(END_Q), os.path.join(tmp, "e%03d.jpg")])
    return [open(f, "rb").read() for f in sorted(glob.glob(os.path.join(tmp, "e*.jpg")))]


def build(photos):
    tmp = tempfile.mkdtemp(prefix="montage_")
    try:
        clips = []
        for i, (name, cx, cy, w, target, n, rate, slide) in enumerate(SHOTS):
            inp, n, rate, moving = source(photos, name, n, rate, tmp)
            out = os.path.join(tmp, "c%d.mp4" % i)
            ffmpeg(inp + ["-vf", shot_filter(cx, cy, w, target, n, rate, slide, i == 0, moving),
                          "-frames:v", str(n), "-c:v", "libx264", "-crf", "14", out])
            clips.append((out, n))
        name, y, n, rate = DETAIL
        inp, n, rate, moving = source(photos, name, n, rate, tmp)
        if moving and len(CLIPS[os.path.splitext(name)[0]]) > 2:
            y = CLIPS[os.path.splitext(name)[0]][2]
        out = os.path.join(tmp, "c%d.mp4" % len(clips))
        ffmpeg(inp + ["-vf",
                      "%sscale=640:-1,crop=640:480:0:%d,%s,zoompan=z='1.0+%s*on':x='(iw-iw/zoom)/2':y='(ih-ih/zoom)/2':"
                      "d=%d:s=320x240:fps=%d,vignette=PI/8,fade=t=out:st=%.1f:d=0.6:color=%s,format=yuv420p"
                      % ("fps=%d," % FPS if moving else "", y, GRADE, rate, 1 if moving else n, FPS, n / FPS - 0.6,
                         "white" if os.path.isfile(os.path.join(photos, END_LOGO)) else "black"),
                      "-frames:v", str(n), "-c:v", "libx264", "-crf", "14", out])
        clips.append((out, n))
        inputs, graph, last, t = [], "", "[0]", clips[0][1] / FPS - XFADE / FPS
        for c, _ in clips:
            inputs += ["-i", c]
        for k in range(1, len(clips)):
            graph += "%s[%d]xfade=transition=fade:duration=%.1f:offset=%.2f[v%d];" % (last, k, XFADE / FPS, t, k)
            last = "[v%d]" % k
            t += clips[k][1] / FPS - XFADE / FPS
        joined = os.path.join(tmp, "montage.mp4")
        ffmpeg(inputs + ["-filter_complex", graph.rstrip(";"), "-map", last, "-c:v", "libx264", "-crf", "14", joined])
        ffmpeg(["-i", joined, "-q:v", "8", "-pix_fmt", "yuvj420p", os.path.join(tmp, "f%03d.jpg")])
        frames = [open(f, "rb").read() for f in sorted(glob.glob(os.path.join(tmp, "f*.jpg")))]
        return frames + end_card(photos, tmp)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


# --cartoon: the same four shots as hand-drawn 2D cartoon stills (each photo redrawn by an image model
# from a "2D cartoon, ink outlines, flat cel colours" prompt), animated here instead of from clips:
# eased push-ins, 0.3 s crossfades, the close-up swapping to its wink still, and a cartoon sparkle
# on the headlight during the wink. Files in the photo dir: <stem>.png (or .jpg) for each shot,
# 06-headlight-detail-wink.png, and honda-logo.png for the end card. Needs Pillow.
# stem, frames, crop centre y (fraction of the still's height), zoom from, zoom to, drift (fraction of width)
CARTOON_SHOTS = [("07-right-wide-turning", 22, 0.497, 1.03, 1.09, -0.021),
                 ("02-front-close", 20, 0.583, 1.00, 1.09, 0.0),
                 ("04-left-closer", 20, 0.577, 1.03, 1.10, 0.017),
                 ("06-headlight-detail", 28, 0.416, 1.00, 1.06, 0.0)]
CARTOON_WINK = ("06-headlight-detail", 12, 5)     # shot, first wink frame, wink frames (as #56)
CARTOON_FADE_IN, CARTOON_FADE_OUT = 8, 6          # frames: from black at the start, to white at the end
# sparkle radius (deck px) from the frame before the wink to the frame after it, and where it sits
SPARKLE = [10, 22, 30, 24, 16, 10, 5]
SPARKLE_AT, SPARKLE_SMALL_AT = (0.47, 0.66), (0.33, 0.71)
SS = 2                                            # supersampling while animating


def cartoon_frames():
    """Frames of the cartoon part: every shot minus the crossfade overlaps."""
    return sum(s[1] for s in CARTOON_SHOTS) - XFADE * (len(CARTOON_SHOTS) - 1)


def ease(t):
    return t * t * (3 - 2 * t)


def cartoon_box(w, h, cy, zoom, drift):
    """The crop (x0, y0, x1, y1) of a w x h still for one 4:3 frame: zoom 1 is the full width,
    centred on height fraction cy, moved by drift (fraction of w), always inside the still."""
    cw = w / zoom
    ch = cw * 240 / 320
    x0 = min(max((w - cw) / 2 + drift * w, 0), w - cw)
    y0 = min(max(cy * h - ch / 2, 0), h - ch)
    return (x0, y0, x0 + cw, y0 + ch)


def cartoon_plan():
    """Per shot, per frame: (zoom, drift, winking, sparkle radius or 0)."""
    stem_w, first, n_wink = CARTOON_WINK
    plan = []
    for stem, n, _, z0, z1, drift in CARTOON_SHOTS:
        shot = []
        for k in range(n):
            e = ease(k / (n - 1))
            wink = stem == stem_w and first <= k < first + n_wink
            s = k - (first - 1)
            spark = SPARKLE[s] if stem == stem_w and 0 <= s < len(SPARKLE) else 0
            shot.append((z0 + (z1 - z0) * e, drift * (e - 0.5), wink, spark))
        plan.append(shot)
    return plan


def cartoon_still(photos, stem):
    for ext in (".png", ".jpg"):
        p = os.path.join(photos, stem + ext)
        if os.path.isfile(p):
            return p
    sys.exit("gen_montage: missing cartoon still %s.png" % os.path.join(photos, stem))


def sparkle(draw, x, y, r, fill):
    """A four-point cartoon star with an ink outline."""
    import math
    pts = [(x + (r if k % 2 == 0 else r * 0.22) * math.cos(k * math.pi / 4),
            y + (r if k % 2 == 0 else r * 0.22) * math.sin(k * math.pi / 4)) for k in range(8)]
    draw.polygon(pts, fill=fill, outline=(20, 10, 30), width=SS)


def build_cartoon(photos):
    from PIL import Image, ImageDraw
    W, H = 320 * SS, 240 * SS
    stem_w = CARTOON_WINK[0]
    shots = []
    for (stem, _, cy, *_), plan in zip(CARTOON_SHOTS, cartoon_plan()):
        img = Image.open(cartoon_still(photos, stem)).convert("RGB")
        wink = Image.open(cartoon_still(photos, stem + "-wink")).convert("RGB") if stem == stem_w else None
        frames = []
        for zoom, drift, winking, spark in plan:
            src = wink if winking else img
            f = src.resize((W, H), Image.LANCZOS, box=cartoon_box(src.width, src.height, cy, zoom, drift))
            if spark:
                d = ImageDraw.Draw(f)
                sparkle(d, SPARKLE_AT[0] * W, SPARKLE_AT[1] * H, spark * SS, (255, 255, 240))
                if spark > 12:
                    sparkle(d, SPARKLE_SMALL_AT[0] * W, SPARKLE_SMALL_AT[1] * H, spark * SS * 0.45, (255, 240, 200))
            frames.append(f)
        shots.append(frames)
    seq = shots[0]
    for s in shots[1:]:
        seq = seq[:-XFADE] + [Image.blend(seq[-XFADE + j], s[j], (j + 1) / (XFADE + 1)) for j in range(XFADE)] + s[XFADE:]
    black, white = Image.new("RGB", (W, H)), Image.new("RGB", (W, H), (255, 255, 255))
    tmp = tempfile.mkdtemp(prefix="montage_")
    try:
        for i, f in enumerate(seq):
            if i < CARTOON_FADE_IN:
                f = Image.blend(black, f, i / CARTOON_FADE_IN)
            if i >= len(seq) - CARTOON_FADE_OUT:
                f = Image.blend(f, white, (i - (len(seq) - CARTOON_FADE_OUT) + 1) / CARTOON_FADE_OUT)
            f.resize((320, 240), Image.LANCZOS).save(os.path.join(tmp, "p%03d.png" % i))
        ffmpeg(["-i", os.path.join(tmp, "p%03d.png"), "-q:v", "8", "-pix_fmt", "yuvj420p", os.path.join(tmp, "f%03d.jpg")])
        frames = [open(f, "rb").read() for f in sorted(glob.glob(os.path.join(tmp, "f*.jpg")))]
        return frames + end_card(photos, tmp)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def pack(frames):
    head = b"MTG1" + struct.pack("<HH", len(frames), FPS)
    offset, index, data, seen = len(head) + 8 * len(frames), b"", [], {}
    for f in frames:
        if f not in seen:                      # repeated frames (the end card's hold) stored once
            seen[f] = offset
            data.append(f)
            offset += len(f)
        index += struct.pack("<II", seen[f], len(f))
    return head + index + b"".join(data)


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if a != "--cartoon"]
    photos = args[0] if args else os.path.join(ROOT, "montage-photos")
    blob = pack(build_cartoon(photos) if "--cartoon" in sys.argv[1:] else build(photos))
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(blob)
    n = struct.unpack("<H", blob[4:6])[0]
    print("gen_montage: %s  %d frames, %.1f s, %d KB" % (os.path.relpath(OUT), n, n / FPS, len(blob) // 1024))
