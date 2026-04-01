# fcitx5-rime-webdav

Fcitx5 plugin for syncing Rime input method user dictionaries across devices via WebDAV.

## Features

- Multi-device dictionary sync via WebDAV protocol
- Automatic conflict resolution with per-device installation IDs
- Incremental sync (only transfers changed snapshots)
- Reinstall recovery (restores dictionaries from remote after fresh install)
- Manual sync via Fcitx5 config tool button
- Startup debug logging for troubleshooting

## Install

### From AUR (Arch Linux)

```bash
yay -S fcitx5-rime-webdav
```

### From Source

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
sudo cmake --install .
```

Then restart fcitx5:

```bash
fcitx5 -rd
```

## Configure

1. Open Fcitx5 configuration tool: `fcitx5-configtool`
2. Find "Rime 词库配置同步" in the addon list
3. Set your WebDAV server URL, username, and password
4. Click "手工同步" button to trigger manual sync

### Supported WebDAV Servers

| Provider | Server URL Format |
|----------|-------------------|
| 坚果云 (Jianguoyun) | `https://dav.jianguoyun.com/dav/` |
| Nextcloud | `https://your-nextcloud.com/remote.php/dav/` |
| Synology | `https://your-nas:port/webdav/` |

### Configuration Fields

| Field | Description |
|-------|-------------|
| ServerURL | WebDAV server address |
| Username | WebDAV login username |
| Password | WebDAV login password |
| SyncEnabled | Enable automatic sync |
| AutoSyncInterval | Auto sync interval (minutes) |
| ManualSyncTrigger | Click to trigger immediate sync |
| LastSyncTime | Auto-updated after each sync |

## Debug Info

The plugin prints Rime path information at startup for debugging. View with:

```bash
fcitx5 -rd 2>&1 | grep rime-webdav
```

## Dependencies

- fcitx5
- librime
- libcurl
- pugixml
- openssl

## License

GPL-3.0-or-later

The bundled webdav-client-cpp library is licensed under the ISC License.
