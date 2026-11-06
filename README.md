# [VIP] Decoy Teleport

This folder contains the source code and project layout for the Pisex VIP plugin described in the attached instructions.

## Structure

- `source/vip_decoy_teleport.h` – plugin interface and exported class
- `source/vip_decoy_teleport.cpp` – core implementation
- `source/AMBuilder` – build script for AMBuild
- `include/` – expected header directory for `vip.h`, `utils.h`, `players.h` copied from the Pisex VIP SDK

## Notes

The actual CS2 SDK dependencies are external and are not included here:

- `hl2sdk-cs2` / `s2sdk`
- `metamod-source`
- `SchemaEntity`
- Pisex VIP `include/` headers

Follow the build steps from the attached notes:

```bash
cd source && mkdir build && cd build
python3 ../configure.py -s cs2 --targets x86_64 --enable-optimize \
  --hl2sdk-manifests=./hl2sdk-manifests \
  --mms_path=/path/to/metamod-source \
  --hl2sdk-root=/path/to/hl2sdk-cs2
ambuild
```

The resulting binary is expected under `build/package/addons/vip_modules/` as `vip_decoy_teleport.so`.
