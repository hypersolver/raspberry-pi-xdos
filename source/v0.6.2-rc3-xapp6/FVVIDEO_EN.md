# FVVIDEO for Raspberry Pi 3B+

FVVIDEO adds GPU memory checks and stops submitting frames when the H.264 decoder reports an error. With a known GPU allocation below 128 MB, the player rejects video larger than 640×480 pixels by pixel count before initializing the decoder.

Back up config.txt before changing the firmware setting. For the tested 720p clip, set gpu_mem=128 and reboot. Keep enough memory for the rest of the system. Restore your previous setting and choose the preserved kernel to roll back. See [installation and recovery](OPERATIONS_ZH.md) and [media controls](../../docs/media-playback.md).

Support depends on the source clip and firmware. This update does not establish 1080p support or recovery from every VideoCore failure.

## Hardware validation

Pi 3B+ at 64 MB refused the 720p test clip and remained responsive. At 128 MB the same clip submitted, decoded and presented all 359 frames. UART PING passed after playback and after restoring 64 MB and FVR19.
