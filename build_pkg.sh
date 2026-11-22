#!/usr/bin/env bash
# Build a native PS4 homebrew Fake PKG with the user's OpenOrbis SDK.
# No SDK files or protected binaries are bundled in this source project.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SDK="${OO_PS4_TOOLCHAIN:-}"
cd "$ROOT"
: "${CC:=clang}"
: "${LLD:=ld.lld}"
CONTENT_ID='IV0000-PWBA00001_00-PS4WEBAUDIO00001'
TITLE_ID='PWBA00001'
NATIVE="$ROOT/native"
OUT="$ROOT/build"
PKGDIR="$OUT/package"
TOOL_BIN=''
fail(){ echo "ERROR: $*" >&2; exit 1; }
[ -n "$SDK" ] || fail 'Set OO_PS4_TOOLCHAIN to the complete OpenOrbis/PS4Toolchain folder.'
[ -d "$SDK/include/orbis" ] || fail "Missing OpenOrbis headers: $SDK/include/orbis"
[ -f "$SDK/lib/crt1.o" ] || fail 'OpenOrbis crt1.o missing. Install a full SDK release, not just the git source.'
for f in libSceVideoOut libScePad libSceUserService libSceSystemService; do
 ls "$SDK/lib/$f".* >/dev/null 2>&1 || fail "Missing SDK stub library: $f"
done
command -v "$CC" >/dev/null || fail "Clang missing; install clang and lld."
command -v "$LLD" >/dev/null || fail 'LLD missing; install clang and lld.'
command -v python3 >/dev/null || fail 'Python3 missing.'
command -v sha256sum >/dev/null || fail 'sha256sum missing.'
for c in "$SDK/bin/linux/create-fself" "$SDK/bin/linux/create-gp4" "$SDK/bin/linux/PkgTool.Core"; do
 [ -f "$c" ] || fail "Missing OpenOrbis packaging utility $c"
done
TOOL_BIN="$SDK/bin/linux"
# Different SDK archives store these support files in different places.
first_existing(){ for path in "$@"; do [ -f "$path" ] && { printf '%s\n' "$path"; return 0; }; done; return 1; }
RIGHT_FILE="$(first_existing "${RIGHT_SPRX:-/nonexistent}" \
 "$SDK/samples/hello_world/sce_sys/about/right.sprx" \
 "$SDK/bin/data/right.sprx" "$SDK/bin/data/about/right.sprx" \
 "$SDK/src/modules/right.sprx" "$SDK/right.sprx" || true)"
LIBC_PRX="$(first_existing "$SDK/bin/data/modules/libc.prx" "$SDK/src/modules/libc.prx" \
 "$SDK/samples/hello_world/sce_module/libc.prx" || true)"
FIOS_PRX="$(first_existing "$SDK/bin/data/modules/libSceFios2.prx" "$SDK/src/modules/libSceFios2.prx" \
 "$SDK/samples/hello_world/sce_module/libSceFios2.prx" || true)"
[ -n "$RIGHT_FILE" ] || fail 'right.sprx not found. Provide RIGHT_SPRX=/absolute/path/to/right.sprx from a trusted OpenOrbis sample.'
[ -n "$LIBC_PRX" ] || fail 'libc.prx not found in SDK (bin/data/modules or src/modules).'
[ -n "$FIOS_PRX" ] || fail 'libSceFios2.prx not found in SDK (bin/data/modules or src/modules).'
# Clean previous package staging so the archived V2 background files cannot leak in.
rm -rf -- "$PKGDIR"
mkdir -p "$OUT" "$PKGDIR/assets" "$PKGDIR/sce_sys/about" "$PKGDIR/sce_module"
sha256sum -c "$ROOT/PAYLOAD_SHA256SUMS"
echo '== 1/5 Compile PS4 native shell =='
"$CC" --target=x86_64-pc-freebsd12-elf -O2 -fPIC -funwind-tables \
  -isysroot "$SDK" -isystem "$SDK/include" -I "$NATIVE" \
  -Wall -Wextra -Wno-misleading-indentation -c "$NATIVE/main.c" -o "$OUT/main.o"
"$LLD" "$OUT/main.o" "$SDK/lib/crt1.o" -m elf_x86_64 -pie --script "$SDK/link.x" \
  --eh-frame-hdr -L "$SDK/lib" -lc -lkernel -lSceVideoOut -lScePad -lSceUserService -lSceSystemService \
  -o "$OUT/PS4WebAudio.elf"
echo '== 2/5 Produce PS4 eboot.bin =='
(
 cd "$OUT"
 "$TOOL_BIN/create-fself" -in=PS4WebAudio.elf -out=PS4WebAudio.oelf \
    --eboot package/eboot.bin --paid 0x3800000000000011
)
cp "$ROOT/assets/icon0.png" "$PKGDIR/sce_sys/icon0.png"
cp "$RIGHT_FILE" "$PKGDIR/sce_sys/about/right.sprx"
cp "$LIBC_PRX" "$PKGDIR/sce_module/libc.prx"
cp "$FIOS_PRX" "$PKGDIR/sce_module/libSceFios2.prx"
cp "$ROOT/assets/BTA-Web-Audio-FW14-V2-NO-BG.bin" "$PKGDIR/assets/BTA-Web-Audio-FW14-V2-NO-BG.bin"
cp "$ROOT/assets/CREDITS.txt" "$PKGDIR/assets/CREDITS.txt"
echo '== 3/5 Generate param.sfo =='
export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1
PKGTOOL="${PKGTOOL:-$TOOL_BIN/PkgTool.Core}"
"$PKGTOOL" sfo_new "$PKGDIR/sce_sys/param.sfo"
sfo(){ "$PKGTOOL" sfo_setentry "$PKGDIR/sce_sys/param.sfo" "$1" --type "$2" --maxsize "$3" --value "$4"; }
sfo APP_TYPE Integer 4 1
sfo APP_VER Utf8 8 01.01
sfo ATTRIBUTE Integer 4 0
# gde non-game category: intended to permit a lightweight companion app.
sfo CATEGORY Utf8 4 gde
sfo CONTENT_ID Utf8 48 "$CONTENT_ID"
sfo DOWNLOAD_DATA_SIZE Integer 4 0
sfo SYSTEM_VER Integer 4 0
sfo TITLE Utf8 128 'PS4 Web Audio - Made by vypr'
sfo TITLE_ID Utf8 12 "$TITLE_ID"
sfo VERSION Utf8 8 01.01
echo '== 4/5 Package and validate =='
(
 cd "$PKGDIR"
 "$TOOL_BIN/create-gp4" -out package.gp4 --content-id="$CONTENT_ID" \
  --files 'eboot.bin sce_sys/param.sfo sce_sys/icon0.png sce_sys/about/right.sprx sce_module/libc.prx sce_module/libSceFios2.prx assets/BTA-Web-Audio-FW14-V2-NO-BG.bin assets/CREDITS.txt'
 "$PKGTOOL" pkg_build package.gp4 "$OUT"
)
[ -f "$OUT/$CONTENT_ID.pkg" ] || fail 'PkgTool did not produce the expected package.'
"$PKGTOOL" pkg_validate "$OUT/$CONTENT_ID.pkg"
cp "$OUT/$CONTENT_ID.pkg" "$ROOT/PS4-Web-Audio-No-BG.pkg"
echo '== 5/5 SHA256 =='
sha256sum "$ROOT/PS4-Web-Audio-No-BG.pkg"
echo "REAL PKG CREATED: $ROOT/PS4-Web-Audio-No-BG.pkg"
