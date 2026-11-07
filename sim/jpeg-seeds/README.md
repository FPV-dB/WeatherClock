JPEG samples generated with ffmpeg (`testsrc2` and `mandelbrot`, ffmpeg's own synthetic images),
with different sampling factors (Y 2x2, and Y 2x2 with chroma 1x2; ffmpeg's 4:4:4, Y 1x2, doesn't decode in the original JPEGDEC either). They are
the seeds for `sim/test-jpeg.sh`: `valid` mode requires every one of them to decode, and fuzzing starts from them.
