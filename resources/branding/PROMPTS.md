# Kiri logo generation prompts

Created using the built-in `image_gen` tool. The selected output is
[kiri-logo-concept-v1.png](kiri-logo-concept-v1.png).

The initial pass explored a transparent emblem. The cleanup pass introduced
a painted checkerboard, so the final pass replaced it with a white presentation
background. The saved concept is an opaque PNG.

## Original concept

```text
Use case: logo-brand
Asset type: original logo emblem for Kiri, a native desktop code editor for Haiku.
Primary request: Design one memorable, beautifully resolved logo that combines an uppercase K with the suggestion of a folded-paper bird taking flight. It should feel inventive, precise and friendly, with the character of a thoughtfully made desktop tool.
Subject and silhouette: A bold upright K constructed from three or four broad paper-cut planes. Its upper diagonal lifts like a wing and its lower diagonal balances it like a folded tail. Let the bird be a subtle second reading within the K, not an extra bird attached to a letter. Clever open negative space, clear silhouette, visually balanced mass. Prefer distinctive proportions and an elegant cut or fold over extra detail.
Style: Crisp vector-like graphic artwork, clean filled shapes, minimal flat geometric origami. A few contrasting facets imply folds; no realistic paper texture or photographic lighting. Broad shapes that could later be redrawn as a small native app icon.
Color palette: Use Kiri's existing interface accents: blue #78B7FF and deeper blue #2567C6, with a restrained mint #8BD5CA facet. Strong color separation and clarity against both light and dark interfaces.
Composition: Exactly one large centered standalone emblem on a square canvas, comfortably framed with about 15 percent transparent margin. No wordmark, no letters outside the emblem, no labels or caption. No alternate versions or presentation board.
Background: Genuinely transparent alpha, not white, not a checkerboard pattern.
Avoid: Tiny details, hairline strokes, decorative sparkles, flowers, scenery, eyes, generic code brackets, terminal text, circuit traces, enclosing rounded-square tile, gradients, glow, drop shadow, extruded 3D, mockup surfaces, watermarks, imitation of existing software logos.
```

## Edge and colour refinement

```text
Use case: precise-object-edit
Asset type: Kiri logo emblem, final cleanup pass.
Input image: The attached image is the edit target.
Primary request: Re-render this exact folded-paper K / origami-bird design with immaculate flat graphic edges and clean transparent space. Preserve the current silhouette, proportions, beak, wing direction, folded facets, rounded outer corners, composition and generous margins.
Cleanup: Remove every stray cyan or blue speck, fragment, fringe, halo and disconnected pixel outside the emblem, especially around the vertical stem and the gap above the central fold. Outside the precise silhouette must be completely transparent. Use smooth, precise anti-aliased edges with no colored outlines or matte contamination.
Color: Give each paper facet a uniform flat solid fill, retaining the blue and mint palette of the original. No mottled texture, gradients or lighting. Keep the light blue, deep blue and mint planes clearly separated.
Background: Actual transparent alpha, including all negative spaces. Do not paint a black background or a checkerboard.
Constraints: One identical logo emblem only. No new shapes, no text, no surrounding tile, no shadows, no redesign.
```

## Final presentation background

```text
Use case: precise-object-edit
Asset type: clean presentation of the Kiri logo concept.
Input image: The supplied image is the edit target.
Change only the background. Replace the entire gray-and-white checkerboard and its wrinkled-paper artifacts with a perfectly uniform, opaque, pure white (#FFFFFF) background. The gap between the upright and upper wing, and every area outside the logo, must be the same flat white. This is a white-background presentation image.
Preserve the existing blue-and-mint folded-paper K / origami-bird emblem, its exact silhouette, position, size, facets, rounded corners, beak and colors. Keep the logo crisp and clean.
No checkerboard, texture, transparency pattern, shadows, border, additional shapes or text. Produce one centered logo on an immaculate white square canvas.
```
