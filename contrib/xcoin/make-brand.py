#!/usr/bin/env python3
"""Generate X Coin brand stills — black/white, Nifty Raven (@NFTRVN) only."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

OUT = Path("/workspace/assets/brand")
OUT.mkdir(parents=True, exist_ok=True)
MKT = Path("/workspace/docs/marketing")
MKT.mkdir(parents=True, exist_ok=True)


def font(size, bold=True):
    candidates = [
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf" if bold else "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf" if bold else "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    ]
    for p in candidates:
        if Path(p).exists():
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()


def draw_x(draw, cx, cy, arm, width, fill=(255, 255, 255)):
    # Sharp X mark (two thick bars)
    pts1 = [
        (cx - arm, cy - arm + width),
        (cx - arm + width, cy - arm),
        (cx + arm, cy + arm - width),
        (cx + arm - width, cy + arm),
    ]
    pts2 = [
        (cx + arm - width, cy - arm),
        (cx + arm, cy - arm + width),
        (cx - arm + width, cy + arm),
        (cx - arm, cy + arm - width),
    ]
    draw.polygon(pts1, fill=fill)
    draw.polygon(pts2, fill=fill)


def logo_mark(size=1024, invert=False):
    bg = (255, 255, 255) if invert else (0, 0, 0)
    fg = (0, 0, 0) if invert else (255, 255, 255)
    im = Image.new("RGB", (size, size), bg)
    d = ImageDraw.Draw(im)
    draw_x(d, size // 2, size // 2, int(size * 0.32), int(size * 0.09), fg)
    return im


def wordmark(w=1600, h=400):
    im = Image.new("RGB", (w, h), (0, 0, 0))
    d = ImageDraw.Draw(im)
    draw_x(d, 160, h // 2, 70, 22, (255, 255, 255))
    f = font(120)
    d.text((280, h // 2 - 70), "X-COIN", font=f, fill=(255, 255, 255))
    sf = font(28, bold=False)
    d.text((280, h // 2 + 70), "Nifty Raven  ·  @NFTRVN", font=sf, fill=(180, 180, 180))
    return im


def app_icon(size=512):
    im = Image.new("RGBA", (size, size), (0, 0, 0, 255))
    d = ImageDraw.Draw(im)
    # sharp square, no rounding — X theme
    d.rectangle([0, 0, size - 1, size - 1], outline=(255, 255, 255), width=8)
    draw_x(d, size // 2, size // 2, int(size * 0.28), int(size * 0.08), (255, 255, 255))
    return im.convert("RGB")


def promo_signin(w=1600, h=900):
    im = Image.new("RGB", (w, h), (0, 0, 0))
    d = ImageDraw.Draw(im)
    draw_x(d, 200, 160, 70, 20)
    f = font(72)
    d.text((320, 110), "X-COIN", font=f, fill=(255, 255, 255))
    f2 = font(36)
    d.text((80, 280), "Sign in with X", font=f2, fill=(255, 255, 255))
    f3 = font(22, bold=False)
    lines = [
        "The handle that gets a free root and lottery eligibility",
        "comes from GET /2/users/me — not from a typed string.",
        "Signing in is what stops impersonation.",
        "",
        "Private test chain  ·  Nifty Raven (@NFTRVN)",
    ]
    y = 360
    for line in lines:
        d.text((80, y), line, font=f3, fill=(200, 200, 200))
        y += 40
    # fake primary button
    d.rectangle([80, 620, 420, 700], fill=(255, 255, 255))
    d.text((110, 638), "Sign in with X", font=font(28), fill=(0, 0, 0))
    return im


def promo_wallet(w=1600, h=900):
    im = Image.new("RGB", (w, h), (0, 0, 0))
    d = ImageDraw.Draw(im)
    draw_x(d, 140, 120, 50, 16)
    d.text((220, 85), "X-COIN", font=font(48), fill=(255, 255, 255))
    d.text((80, 200), "Anyone can use it. No CLI.", font=font(32), fill=(255, 255, 255))
    cards = [
        (80, 300, "RECEIVE", "Show address. Copy."),
        (560, 300, "SEND", "Paste address. Amount. Send."),
        (1040, 300, "LOTTERY", "Eligible? Next draw."),
        (80, 560, "MY ASSET", "Root from signed-in handle."),
        (560, 560, "ISSUE", "Sub / unique. Buttons."),
        (1040, 560, "SIGN IN", "OAuth PKCE + users/me."),
    ]
    for x, y, title, body in cards:
        d.rectangle([x, y, x + 440, y + 200], outline=(255, 255, 255), width=2)
        d.text((x + 24, y + 28), title, font=font(22), fill=(255, 255, 255))
        d.text((x + 24, y + 90), body, font=font(18, bold=False), fill=(180, 180, 180))
    d.text((80, 840), "Nifty Raven  ·  @NFTRVN  ·  private test 1.0", font=font(18, bold=False), fill=(120, 120, 120))
    return im


logo_mark().save(OUT / "logo-mark.png")
logo_mark(invert=True).save(OUT / "logo-mark-on-white.png")
wordmark().save(OUT / "wordmark.png")
app_icon().save(OUT / "app-icon.png")
app_icon(1024).save(OUT / "app-icon-1024.png")
promo_signin().save(OUT / "promo-signin.png")
promo_wallet().save(OUT / "promo-wallet.png")

# SVG mark
(OUT / "logo-mark.svg").write_text(
    """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1024 1024">
<rect width="1024" height="1024" fill="#000"/>
<polygon fill="#fff" points="220,280 280,220 804,744 744,804"/>
<polygon fill="#fff" points="744,220 804,280 280,804 220,744"/>
</svg>
""",
    encoding="utf-8",
)

readme = """# X Coin brand

**Nifty Raven** (@NFTRVN on X) only. No legal name.

X theme: black, white, sharp X mark. Private test 1.0.

| File | Use |
| --- | --- |
| `logo-mark.png` / `logo-mark.svg` | X mark on black |
| `logo-mark-on-white.png` | X mark on white |
| `wordmark.png` | Mark + X-COIN |
| `app-icon.png` | Desktop icon |
| `promo-signin.png` | Sign in still |
| `promo-wallet.png` | Product still |

Do not publish this chain. Do not add public DNS seeds.
"""
(OUT / "README.md").write_text(readme, encoding="utf-8")
(MKT / "README.md").write_text(
    "Brand files live in [`assets/brand/`](../../assets/brand/).\n\nCredit: **Nifty Raven (@NFTRVN)** only.\n",
    encoding="utf-8",
)
print("wrote", list(OUT.iterdir()))
