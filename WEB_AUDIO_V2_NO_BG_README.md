# PS4 Web Audio V2 — Standard Playback (No Background)

This is a separate FW14 RAM payload based on the working Web Audio V2 capture/streaming engine. The experimental live-WAV background player, virtual duration, byte-range handler, live-edge seek controls, and associated endpoint have been removed.

## Included

- Standard **Listen to PS4** low-latency browser player, network test tone, volume slider.
- 3 audio quality profiles: Fastest 24 kHz mono PCM16, Balanced 48 kHz stereo PCM16, Highest quality 48 kHz stereo Float32.
- Up to 8 independent listeners and disconnect-safe socket handling from V2.
- Phone auto-detection and local manual override (iPhone/iPad / Android / desktop).
- `navigator.audioSession.type='playback'` on iPhone when supported, before starting AudioContext; does not promise screen-off playback.
- Working V2, V1 and V14 rollback payloads.

## Install (WSL)

1. Put the ZIP into `C:\Users\vypr\Downloads`.
2. On PS4 enable GoldHEN Payload Server port 9090 and FTP port 2121.
3. In WSL, run:

```bash
set +e
mkdir -p "$HOME/fw14-web-audio-no-bg"
unzip -o -q /mnt/c/Users/vypr/Downloads/FW14_PS4_WEB_AUDIO_V2_NO_BACKGROUND.zip -d "$HOME/fw14-web-audio-no-bg"
cd "$HOME/fw14-web-audio-no-bg"
sha256sum -c SHA256SUMS
python3 tools/deploy_web_audio.py BTA-Web-Audio-FW14-V2-NO-BG.bin --host 192.168.1.104
```

4. Browse to **http://192.168.1.104:8098/**. Press **Listen to PS4**.

## Verification

From the included `source` directory, `make test` and `node tests/test_phone_mode.js && node tests/test_ios_audio_session.js && node tests/test_no_background.js`.

This variant has not been hardware-tested on a PS4. The original Web Audio V2 was the tested network streaming baseline. With background playback removed, screen-off audio on iPhone is not guaranteed.
