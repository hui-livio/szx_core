---
name: opencv-cpp-aoi
description: Use this skill whenever working with OpenCV in C++ projects, especially AOI, PCB inspection, image preprocessing, defect detection, template matching, camera/image IO, ROI processing, contour analysis, morphology, calibration, or performance-sensitive computer vision code.
---

# OpenCV C++ AOI

Use this skill for C++ OpenCV image-processing work, with emphasis on AOI and PCB inspection systems.

## Engineering Rules

1. Understand the image pipeline before editing:
   - input format, color space, bit depth
   - ROI coordinate conventions
   - preprocessing steps
   - thresholds and calibration data
   - expected output structures
2. Be explicit about color conversions. Do not assume RGB/BGR/gray without checking the surrounding code.
3. Keep coordinate systems consistent. Name variables clearly when converting between full-image, ROI-local, and display coordinates.
4. Prefer deterministic operations for inspection logic. Avoid hidden randomness in production detection paths.
5. Preserve source images unless mutation is intended. Use `clone()` only when mutation or lifetime requires it.
6. Check empty inputs and invalid ROIs before calling OpenCV operations.
7. For performance-sensitive code:
   - avoid unnecessary allocations inside per-frame loops
   - reuse buffers where the codebase already has that pattern
   - prefer ROI views over image copies when safe
   - measure before replacing clear code with complex optimizations
8. Keep visualization/debug drawing separate from detection logic unless the existing code intentionally combines them.

## Common AOI Patterns

- Preprocessing: denoise, normalize illumination, grayscale conversion, thresholding, morphology.
- Localization: template matching, feature matching, contours, connected components, line/circle detection.
- Defect checks: missing part, offset, polarity, solder bridge, contamination, scratch, dimension tolerance.
- Measurement: keep pixel-to-world calibration explicit and document units.

## Testing Guidance

- Add tests with small synthetic images for geometry and threshold edge cases.
- Add fixture image tests only when the repository already stores test assets or the user provides images.
- Verify empty image, out-of-bounds ROI, all-black/all-white image, and noisy boundary cases.

## Output

When reporting changes, include:

- affected pipeline stage
- image assumptions used
- edge cases handled
- performance tradeoffs
- how the result was verified
