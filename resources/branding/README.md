# Kiri branding

## Haiku application icon

![Kiri's native application icon](kiri-icon-preview.png)

The application icon adapts the approved folded-paper K and origami bird to
[Haiku's icon guidelines](https://www.haiku-os.org/development/icon-guidelines/).
Its front edges use the 2:1 perspective, verticals remain vertical, and broad
blue and mint folds carry the shape. Lighting comes from the upper left; the
small floor shadows extend straight right. The black silhouette is one pixel
wide at 32 × 32. The background is transparent.

- [HVIF](kiri-icon.hvif): the native vector icon embedded by `resources/Kiri.rdef`.
- [Icon-O-Matic document](kiri-icon.icon): editable native paths and gradients.
- [SVG source](kiri-icon.svg): the vector drawing used for the native import.
- Transparent PNG renders: [16](kiri-icon-16.png), [32](kiri-icon-32.png),
  [64](kiri-icon-64.png) and [256](kiri-icon-256.png) pixels.

| 16 × 16 | 32 × 32 | 64 × 64 |
| --- | --- | --- |
| ![16 px](kiri-icon-16.png) | ![32 px](kiri-icon-32.png) | ![64 px](kiri-icon-64.png) |

The SVG was imported into Haiku R1/beta5's Icon-O-Matic and exported as HVIF;
the PNGs use Haiku's native `BIconUtils` renderer. To revise the icon, open the
SVG or native document in Icon-O-Matic, save the native document, then choose
**File → Export as… → HVIF** and replace `kiri-icon.hvif`. Keep the editable
sources in sync. Both native build systems track the HVIF as a link dependency.

To regenerate previews on Haiku, run from the repository root:

```sh
g++ -std=c++17 tools/render-icon.cpp -o build-haiku/render-icon -lbe -ltranslation
for size in 16 32 64 256; do
    build-haiku/render-icon resources/branding/kiri-icon.hvif \
        resources/branding/kiri-icon-$size.png "$size"
done
build-haiku/render-icon resources/branding/kiri-icon.hvif \
    resources/branding/kiri-icon-preview.png 256 light
```

Omit `light` for transparency, or use `dark` for a dark-background preview.

## Approved logo concept

<img src="kiri-logo-concept-v1.png" alt="Blue and mint Kiri logo: a folded-paper K with an origami bird" width="384">

A folded-paper **K** with an origami-bird detail, inspired by Kiri's blue and
mint interface accents. Broad facets and an open silhouette give the mark a
simple, recognisable shape.

[Logo concept PNG](kiri-logo-concept-v1.png): 1254 × 1254 pixels, on a white
background. Created with the built-in image generation tool; the
[generation prompts](PROMPTS.md) record the design and refinement passes.
