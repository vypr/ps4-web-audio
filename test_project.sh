#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname -- "$0")"
cc -std=c11 -Wall -Wextra -Werror -Wno-misleading-indentation -I native native/host_preview.c -o /tmp/ps4-web-audio-ui-preview
/tmp/ps4-web-audio-ui-preview /tmp/ps4-web-audio-ui-preview.ppm
python3 - <<'PY'
from pathlib import Path
from PIL import Image
p=Path('assets/BTA-Web-Audio-FW14-V2-NO-BG.bin')
assert p.stat().st_size > 1024
im=Image.open('assets/icon0.png'); assert im.size==(512,512)
assert Path('/tmp/ps4-web-audio-ui-preview.ppm').stat().st_size > 2_000_000
src=Path('native/main.c').read_text()
for s in ('8098','9090','/data/ps4_web_audio/stop','/app0/assets/BTA-Web-Audio-FW14-V2-NO-BG.bin',
          'detect_lan_ip','queue_restart','tick_restart'):
    assert s in src,s
from hashlib import sha256
assert sha256(p.read_bytes()).hexdigest()=="27ccf538e1761445dee009aafc0a9ae4798d16332762ad6241ceee37786d986e"
page=Path('WebAudioV2_NoBG_source/app/web.html').read_text()
worker=Path('WebAudioV2_NoBG_source/app/worker.c').read_text()
assert 'id="listen"' in page
assert 'Made by <strong>vypr</strong>' in page
assert 'https://github.com/vypr' in page
assert 'wa_render(&web_audio_status,lan_ip,notice);' in src
assert 'wa_render(&stat' not in src
ui=Path('native/ui.h').read_text()
assert 'wa_controller_icon' in ui and 'wa_control_row' in ui
assert all(x in ui for x in ('WA_BTN_CROSS','WA_BTN_SQUARE','WA_BTN_TRIANGLE'))
assert 'WA_BTN_CIRCLE' not in ui
assert 'ORBIS_PAD_BUTTON_CIRCLE' not in src
preview=Image.open('/tmp/ps4-web-audio-ui-preview.ppm').convert('RGB')
for point, expected in [((864,256),(0x67,0xb5,0xff)), ((864,313),(0xf0,0x9b,0xde)), ((864,383),(0x7b,0xf0,0xa8))]:
    assert preview.getpixel(point)==expected,(point,preview.getpixel(point),expected)
assert 'sfo APP_VER Utf8 8 01.01' in Path('build_pkg.sh').read_text()
assert 'sfo TITLE Utf8 128 \'PS4 Web Audio - Made by vypr\'' in Path('build_pkg.sh').read_text()
assert '-lSceLncUtil' not in Path('build_pkg.sh').read_text()
assert 'navigator.audioSession.type=' in page
for phrase in ('Background playback','Jump to Live','/stream.wav'):
 assert phrase not in page and phrase not in worker,phrase
print('PASS: V2 No-BG untouched, 3 vector PS controller symbols, simpler UI, vypr branding, host preview.')
PY
