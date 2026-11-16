# Art to sprite: the production line for new item pictures

Decided with the user on 2026-11-15, with the skull key (object 270) as the test.
The user liked the nearest-colour result ("the nearest is neat") and asked to keep this
line for when we start using it. **Nothing from it is in the game yet.**

## What it is for

UW's object sprites are 16x16. AI upscalers can't add detail that isn't there; see "What we
tried" below. So the line starts from a **new, detailed picture of the item**, made by an
image generator, and brings it down to sprite size in the original sprite's own colours.

## The steps

Script: `tools/art_to_sprite.py`

```
python tools/art_to_sprite.py ART.png OBJ [--out DIR] [--sizes 1,2,4,8,16]
```

Creatures, or art on a non-white plain background (added 2026-11-15 for creatures 06, 21, 27 and 30):

```
python tools/art_to_sprite.py ART.png creature-06 --sprite dist/UW1/data/sprites/creatures/06/p0_000.png --bg corner --sizes 1,2,4 --out work/art/creature-06
```

`--bg corner` takes the background colour from the top-left pixel. `--tol 24` sets how far a pixel
can be from that colour and still count as background. Creature frames aren't square (06 is
35x45), so 4x is 140x180. The user's 4-creature sheet was cropped into
`work\art\sources\creature-NN.png` first, with the labels cut off.

The Python needs numpy and pillow, for example `O:\appsnotonc\pixelup\.venv\Scripts\python.exe`.

1. **Get the art.** Make a detailed pixel-art style picture of the item on a plain white
   background, in the same pose and direction as the original sprite. The skull key's art
   was 1024x576, key lying flat with the head on the left, like sprite 270. It works best
   when the art's colours are already close to the original's: the user's third key was
   recoloured toward the original tans, and 86% of its pixels were within 25 (RGB) of the
   original's 9 colours. Keep the source in `work\art\sources\<OBJ>-<name>.png`.
2. **Cut-out.** White touching the border is background. An enclosed white patch over 200 px
   is a hole through the item, such as the key's bit, and is background too. Small white
   specks inside the item stay; they are highlights.
3. **Fit.** The cut-out is scaled with Lanczos, using premultiplied alpha so no white fringe
   creeps in, until it fits the original sprite's opaque box times the factor. It is
   centred there on a (16 x factor) square, so it lies exactly where the original item
   lies. Key 270's box is 16x7, so at 64x64 the key is 64x28 in the lower half.
4. **Alpha.** Cut at 128, so every pixel is fully on or off, as all UW sprites are.
5. **Nearest colour (the keeper).** Every opaque pixel is set to the nearest, by RGB
   distance, of the original sprite's own colours, with no dithering. Key 270 has 9:
   black plus 8 tans and browns.

## Outputs

Default folder: `work\art\<OBJ>`. If that folder is already used, a new folder `-2`, `-3`
and so on is made instead; nothing is overwritten.

| File | What it is |
|---|---|
| `<OBJ>-cutout.png` | Step 2 at full size, transparent. |
| `<OBJ>-x<f>-<16f>px.png` | Steps 3-4 in full colour. |
| `<OBJ>-x<f>-<16f>px-origcolours.png` | Step 5. **Use this one.** |
| `<OBJ>-sheet.png` | The original beside the full-colour row and the nearest-colour row, each shown at 320 px. |

## Size

The target is **64x64**, 4x the sprite. Findings from the skull key:

| Size | Result |
|---|---|
| 16x16 | Too small: the art becomes a blob. The hand-drawn original reads better. |
| 32x32 | Recognisable but blurry. Nearest colours help. |
| 64x64 | Skull, eye sockets, teeth, shaft and bit all clear. Chosen size. |
| 128, 256 | Near full detail. Kept for reference. |

The art has no black outline, so at 32 px and below it fades into the background. A 1 px
outline is an option there; it hasn't been tried yet.

## Still to do (when we "get it going")

- **Art for more items.** Either the user supplies it, or we install an image-editing model
  in ComfyUI on O:, which would draw the art from the original 16x16 sprite plus a text
  instruction. Candidates: Qwen-Image-Edit or FLUX.1 Kontext dev. Only a video model is
  installed now (minimax h3).
- **Wiring 64x64 sprites into the renderer.** The sprite atlas is 1024x8192 at 1x, so 4x
  sprites need their own atlas or layout, like `hatlas` for walls and floors. Add an
  Esc-menu row to switch it, as every new part gets.

## What we tried before this (2026-11-14/15), and why it was dropped

All of these work on the original 16x16 directly, and none can invent detail:

- **AI upscalers.** We tried 4x-UltraSharp, Remacri, Siax, RealESRGAN, RealESRGAN anime,
  AnimeSharp, ClearRealityV1, Nomos8kSC, LSDIR, HDCube, and the pixel-art models
  PixelPerfectV4, PocketMonsters-Alpha, Fatality, Fatality-MK2 and NXbrz.
  - The results were smooth or gritty, but invented odd texture.
  - Pre-scaling the sprite blocky (2x or 4x) before the AI made it worse: the models keep
    the blocks.
  - Test sheets are in `work\aitest\`.
- **Rule-based scalers:** MMPX, xBRZ, hq4x and Scale4x, with the outline stripped and put
  back 1 px thick. The tools are in `O:\appsnotonc\pixelup`.
- **The full sprite run.** It was stopped and all its outputs were deleted at the user's
  request. Only the upscaled walls and floors are in the game (Esc menu > Textures).

The skull key's earlier rounds are kept in `work\aitest\skullkey`. `v1-*` is the key with a
ring around the skull, on object 270. `v2-*` is the bare skull before recolouring. The
`260-*` files are the first try, fitted to the wrong key.
