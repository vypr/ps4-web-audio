# PS4 Web Audio - Simple controls UI (PKG update 01.01)

**What changed:** The native PS4 app now uses four large, directly drawn PlayStation controller shapes (blue cross, pink square, green triangle, red circle) with simple English labels: Start, Stop, Refresh, Close. Simplified main screen and status text, retaining the server URL, online status, listener count and audio-capture state.

**What did not change:** V2 No Background streaming payload, web player, device auto-detection, audio quality modes, eight-device listener limit, GoldHEN loader behavior, IP address/port, title ID `PWBA00001`, vypr attribution, and icon. The existing working payload is byte-for-byte unchanged.

PS4 app version is now **01.01** (previously 01.00); stream engine remains **V2**.

## Build in existing Ubuntu WSL

```bash
set +e
cd "$HOME/ps4-web-audio-simple/PS4_Web_Audio_No_BG_VYPR_SIMPLE_UI_Project"
export OO_PS4_TOOLCHAIN="$HOME/openorbis-setup/sdk/OpenOrbis/PS4Toolchain"
bash test_project.sh
SSL="$HOME/ps4-pkg-ssl/extracted/usr/lib/x86_64-linux-gnu"
if [ -f "$SSL/libssl.so.1.1" ]; then
  LD_LIBRARY_PATH="$SSL${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" bash build_pkg.sh
else
  bash build_pkg.sh
fi
```

If successful, this produces `PS4-Web-Audio-No-BG.pkg`. Copy it into Windows Downloads. If GoldHEN refuses to overwrite the existing title, close and uninstall the old home-screen app first, then install the new PKG; this does not require deleting `/data/ps4_web_audio` or the existing payload file. Keep backup files.

This project includes source and bundle, not a prebuilt PS4 `.pkg`. The final packaging and runtime behavior must be tested with your WSL OpenOrbis setup/PS4.
