# ArkRestApi

AsaApi plugin that exposes server actions over a REST HTTP API secured with a Bearer token.
Built on `Poco::Net::HTTPServer` (already a vcpkg dependency of AsaApi core). All game calls run
on the main game thread — HTTP requests are queued and executed on the next tick, so it's safe
even though the HTTP server runs on its own thread pool.

## Build

1. Open `AsaApi.sln` — the `ArkRestApi` project was added alongside `AsaApi` and depends on it
   (build order is already set).
2. Build `AsaApi` first (Release|x64), then build `ArkRestApi`. It links against
   `out_lib\AsaApi.lib`, produced by AsaApi's post-build step.
3. If this is the first time building with vcpkg manifest mode in this project folder, `vcpkg`
   will fetch/build `poco[netssl]` for the `x64-windows-1439-static-md` triplet the first time —
   this can take a while. Requires `vcpkg integrate install` to have been run once on the machine
   (same requirement as the core AsaApi project).
4. The post-build step copies `ArkRestApi.dll`, `PluginInfo.json` and `config.json` into
   `_Deploy\ArkApi\Plugins\ArkRestApi\` at the solution root, ready to copy to the server.

## Deploy

Copy the folder `_Deploy\ArkApi\Plugins\ArkRestApi\` to your server at:

```
<ServerDir>\ShooterGame\Binaries\Win64\ArkApi\Plugins\ArkRestApi\
```

Edit `config.json` there and set a real `BearerToken` (long random string) — the plugin refuses
to start if it's left as the placeholder or empty. `Port`/`BindAddress` control where it listens;
`0.0.0.0` binds all interfaces, so put this behind a firewall/VPN unless you add TLS in front of it
(there's no HTTPS termination in the plugin itself — run it behind a reverse proxy, or restrict
`BindAddress`/firewall rules to trusted IPs, if it's reachable from the internet).

## Authentication

Every route except `GET /health` requires:

```
Authorization: Bearer <BearerToken from config.json>
```

## Endpoints

All bodies/responses are JSON. Player selectors (`kick`, `ban` aside) accept **one of**
`steamName`, `eosId` or `playerId` in the request body.

| Method | Path | Body | Notes |
|---|---|---|---|
| GET | `/health` | - | No auth. Liveness check. |
| GET | `/api/v1/status` | - | Server status + online player count. |
| GET | `/api/v1/players` | - | List of online players, each with `playerId`, `steamName`, `characterName`, `eosId`, `level`, `tribeId`, `tribeName`, `ip`, `isDead`, `position`. |
| POST | `/api/v1/players/kick` | `{"steamName":"..."}` or `{"playerId":..., "reason":"..."}` | |
| POST | `/api/v1/players/ban` | `{"steamName":"...", "durationMinutes":0}` | `durationMinutes: 0` = permanent. |
| POST | `/api/v1/broadcast` | `{"message":"...", "alsoChat":false}` | Server message to all; `alsoChat` also sends as chat. |
| POST | `/api/v1/players/message` | `{"steamName":"...", "message":"...", "senderName":"Server"}` | Chat message to one player. |
| POST | `/api/v1/players/teleport` | `{"steamName":"...", "x":0,"y":0,"z":0}` | |
| POST | `/api/v1/players/teleport-to-player` | `{"from":{"steamName":"a"},"to":{"steamName":"b"},"checkForDino":true,"maxDistance":-1}` | |
| POST | `/api/v1/spawn/dino` | `{"blueprint":"Blueprint'/Game/.../Dino_C'","nearPlayer":{"steamName":"..."},"level":1,"forceTame":false}` | `x/y/z` instead of `nearPlayer` to spawn at coords. |
| POST | `/api/v1/spawn/item` | `{"blueprint":"...","x":0,"y":0,"z":0,"amount":1,"quality":0}` | Drops the item on the ground near the coords. |
| POST | `/api/v1/players/give-item` | `{"steamName":"...","blueprint":"...","quantity":1,"quality":0,"autoEquip":false}` | Gives the item directly into the player's inventory. |
| POST | `/api/v1/players/give-engrams` | `{"steamName":"...","forceAll":true,"tekOnly":false}` | Unlocks engrams for the player. |
| POST | `/api/v1/players/give-exp` | `{"steamName":"...","amount":1000,"fromTribeShare":false,"preventSharingWithTribe":false}` | |
| POST | `/api/v1/players/set-level` | `{"steamName":"...","level":100}` | Sets the player's level directly. |
| POST | `/api/v1/players/clear-inventory` | `{"steamName":"...","clearInventory":true,"clearSlotItems":true,"clearEquippedItems":true}` | |
| POST | `/api/v1/players/god` | `{"steamName":"..."}` | **Toggle** — calling it again turns God Mode back off. |
| GET | `/api/v1/players/{steamName}/inventory-count?item=ItemName` | - | |
| GET | `/api/v1/players/{steamName}/tribe` | - | Returns `tribeId` and `tribeName`. |
| POST | `/api/v1/world/save` | - | |
| POST | `/api/v1/world/destroy-all-enemies` | - | Destroys all wild/hostile dinos on the map. Destructive, global action. |
| POST | `/api/v1/world/time` | `{"time":"1200"}` | Sets the map's time of day. |

## Examples

```bash
TOKEN="your-bearer-token"
HOST="http://127.0.0.1:8766"

curl "$HOST/health"

curl -H "Authorization: Bearer $TOKEN" "$HOST/api/v1/players"

curl -X POST -H "Authorization: Bearer $TOKEN" -H "Content-Type: application/json" \
  -d '{"message":"Server restarting in 10 minutes"}' \
  "$HOST/api/v1/broadcast"

curl -X POST -H "Authorization: Bearer $TOKEN" -H "Content-Type: application/json" \
  -d '{"steamName":"SomePlayer","reason":"AFK too long"}' \
  "$HOST/api/v1/players/kick"

curl -H "Authorization: Bearer $TOKEN" "$HOST/api/v1/players/SomePlayer/tribe"
```

## Extending

Add new actions in `Source/Routes.cpp` (the game-thread logic), wire the route in
`Source/ApiRouter.cpp`, and document it above. Every `Routes::` function may throw
`RestApiError(httpStatus, message)` for expected failures (bad input, player not found, ...).
