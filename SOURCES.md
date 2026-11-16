# Sources

The published work, measurements and references behind the changes uwr makes to the game. The software we build on
and its licences are in [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md); this file is about where the ideas and
the numbers come from.

It was collected from the comments in the code and from our notes. Where the notes name a source without its full
details, it is listed here as the notes name it, and marked so.

## The game itself

The rules, the numbers and the original's look come from the game's own files, not from estimates.

- **UW.EXE** (Ultima Underworld: The Stygian Abyss, 1992) and its data files. The light levels and how far each one
  reaches are SHADES.DAT's rows; the shading by distance is LIGHT.DAT's maps (`src/shaders/common.hlsli`). The
  mechanics are in [MECHANICS.md](MECHANICS.md) and [ROUTINES.md](ROUTINES.md).
- **Underworld Exhumed** by abedegno, a byte-matching rebuild of UW.EXE in C
  (https://github.com/abedegno/underworld-exhumed). It is the rules core, and the first place we look for how the
  original did something.
- **Ultima Underworld Editor** by Karl Clinckspoor
  (https://github.com/KarlClinckspoor/UltimaUnderworldEditorAvalonia), for checking the levels our game saves.

## Fire and flames

The flame model is in `src/render.c` (`render_flame`, `flame_breath`).

- **How fast a flame puffs**, f = 1.5 / sqrt(D): B. M. Cetegen and T. A. Ahmed, "Experiments on the periodic
  instability of buoyant plumes and pool fires", Combustion and Flame 93 (1993), 157-184.
  https://www.frames.gov/catalog/11705
- The same law from vortex dynamics, and its small dependence on the fuel: "Flickering of buoyant diffusion flames:
  a vortex dynamics revisit", https://arxiv.org/pdf/1803.10400. An independent derivation that gives nearly the same
  number: https://shepherd.caltech.edu/EDL/publications/reprints/AM_JFM_PuffingFlames_August_2013.pdf
- **How tall a flame is**, L = 0.235 Q^0.4 - 1.02 D: G. Heskestad, "Luminous heights of turbulent diffusion
  flames", Fire Safety Journal 5 (1983), 103-108.
- **How bright a puff glows**: the Stefan-Boltzmann law (emission by the fourth power of the temperature), the light
  being soot glowing as a black body.
- **How a moving flame leans**: a Froude-number relation between the air's speed and the flame's height (the
  comment in the code gives the formula; our notes do not name a paper for it).
- **The slow breath** (a sag or a flare of a few seconds, now and then, without a rhythm). Flames' light has slow,
  aperiodic changes below about 1 Hz besides the puffing, from the flame's shape changing with the air round it and
  the fuel reaching it. The evidence for this slow part is thinner than for the puffing: it is described, but we
  found no measured slow spectrum for a torch.
  - An infra-red flame detector patent, US 4471221: most of a flame's flicker energy lies between DC and 5 Hz.
    https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/4471221
  - "On-line flicker measurement of gaseous flames by image processing and spectral analysis": the slow components
    put down to changes in the flame's geometry from aerodynamic and convective effects.
    https://www.researchgate.net/publication/230940940_On-line_Flicker_Measurement_of_Gaseous_Flames_by_Image_Processing_and_Spectral_Analysis
  - An illumination-source identification patent, US 12480811: oil and gas lamps fluctuate randomly, not
    periodically. https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/12480811
  - "The structure and radiation of an ethanol pool fire", Combustion and Flame (1987): low-frequency, large-scale
    fluctuations matter for a fire's radiation. https://www.sciencedirect.com/science/article/abs/pii/0010218087901106
  - Campfire flicker measured in the field (about 2.7 Hz), a write-up of Hobaiter and Dominy's work:
    https://scienceblog.com/s-two-researchers-who-measured-real-campfires-report-that-firelight-is-bright-enough-to-read-a-pair-of-hands-and-argue-that-its-flicker-may-be-what-disadvantaged-gesture-and-favoured-speech/

## The colour of firelight

In `src/render.c` (`render_blackbody`, `cam16_degree`, the OKLab functions).

- **A flame's colour from its temperature**: Planck's law, through the CIE 1931 colour matching functions.
- The fit to those functions: C. Wyman, P.-P. Sloan and P. Shirley, "Simple analytic approximations to the CIE XYZ
  color matching functions", Journal of Computer Graphics Techniques 2(2), 2013.
- **The eye getting used to firelight** (the Light colour and Light adaptation rows): CAT16 and the degree of
  adaptation from CAM16: C. Li and others, "Comprehensive color solutions: CAM16, CAT16, and CAM16-UCS", Color
  Research and Application 42(6), 2017.
- The other choice on the Light colour row: the Bradford chromatic adaptation transform.
- **Colours outside the screen's range** brought in by lowering their chroma in OKLCh: Björn Ottosson, "A perceptual
  color space for image processing" (OKLab), 2020. https://bottosson.github.io/posts/oklab/
- sRGB, the D65 white, and for HDR output scRGB (1.0 = 80 nits) and HDR10 (ST 2084) on Linux.

## Surfaces, water and relief

In `src/shaders/scene.hlsl` and [NOTES.md](NOTES.md).

- The sheen on stone: the Blinn-Phong highlight (J. Blinn, "Models of light reflection for computer synthesized
  pictures", 1977).
- Water: reflection by Fresnel's law, the bed seen through it dimmed by the Beer-Lambert law.
- Relief on walls (parallax occlusion mapping): N. Tatarchuk, "Practical parallax occlusion mapping", 2006. Our
  notes also name, without further details: PlayCanvas, the Dark Mod wiki, Unreal's and Unigine's pixel depth
  offset, and "Hoetzlein 2025".
- The normal, height and occlusion maps are made with Laigter (https://github.com/azagaya/laigter).

## Sound and acoustics

In `port/win32/uwacoustic.h` and `docs/superpowers/plans/2026-11-13-sound-and-acoustics.md`.

- **How long a room rings**: Sabine's formula, RT60 = 0.161 V / (S a) (W. C. Sabine, about 1900), with the room's
  size taken from the mean free path of traced rays (4 V / S).
- Acoustics High: Valve's Steam Audio 4.8.1. Reverb and 3D sound otherwise: OpenAL Soft's EFX and HRTF.
- The music: Munt (the Roland MT-32) and Nuked OPL3 (the AdLib), as the original's own drivers used those devices.

## Creatures

In `docs/superpowers/specs/2026-11-16-creature-ai-levels-design.md`, [LLM_NPC_AI.md](LLM_NPC_AI.md) and
`docs/superpowers/plans/2026-11-13-creature-decider-finetune.md`.

- The coded AI levels follow what the design spec took from the published accounts of the AI of F.E.A.R., Halo and
  Descent: the smart-looking behaviour came from simple rules, a table per creature type, a squad layer and visible
  reactions (the spec names the games, not the individual papers).
- The language-model creatures run on llama.cpp (https://github.com/ggml-org/llama.cpp). The models looked at are
  listed in LLM_NPC_AI.md (LFM2.5 by Liquid AI, Qwen, Gemma, Phi), with where each figure came from.
- The fine-tuning plan follows the decision-model projects named in that plan (as the plan names them: AWS Strands
  Decider, Mapika decider, Together Tev1, Geuter et al.).

## The picture

- DLSS, Ray Reconstruction and DLSS 5: NVIDIA's DLSS SDK and its programming guides.
- Vulkan: the Khronos Vulkan specification and its validation layers; HDR on Linux through VK_EXT_swapchain_colorspace
  and VK_EXT_hdr_metadata (`docs/LINUX_RENDERER.md`).
