# ArkRestApi — Node.js panel integration (registration flow)

Reference implementation for: *player connects to the ARK server → panel notices they
aren't registered → sends them a Discord link/code via in-game DM → player registers on
Discord → panel delivers welcome rewards next time they're seen online.*

## Flow

```
ARK server                  Node panel (this code)              Discord bot
    |                              |                                  |
    | <--- GET /api/v1/players --- | (poll every ~20s)                |
    |                              |                                  |
    |                              | player.eosId not registered?     |
    |                              | -> create code, save w/ eosId    |
    | <--- POST .../message ------ | "entre no Discord, /registrar X" |
    |                              |                                  |
    |                              |          /registrar X   ---->    | bot validates code,
    |                              |                                  | links Discord<->eosId,
    |                              | <---------- marks registered --- | queues welcome reward
    |                              |                                  |
    | <--- POST .../give-item ---- | delivers queued reward           |
    |      (next poll where        |    when player is online         |
    |       player is online)      |                                  |
```

The poller only *reports and delivers* — it never decides what "registered" means or what
the welcome reward is. That's owned by your database via the `RegistrationRepository`
interface in `registrationPoller.ts`. Implement its 5 methods against whatever you already
use (Prisma, TypeORM, raw SQL...).

## Wiring it up

```ts
import { ArkRestApiClient } from "./arkRestApiClient";
import { startRegistrationPoller, RegistrationRepository } from "./registrationPoller";

const api = new ArkRestApiClient({
	baseUrl: "http://your-ark-server-ip:8766",
	bearerToken: process.env.ARK_REST_API_TOKEN!,
});

const repo: RegistrationRepository = {
	async isRegistered(eosId) {
		return (await db.player.findUnique({ where: { eosId } }))?.registered ?? false;
	},
	async wasNotified(eosId) {
		return (await db.player.findUnique({ where: { eosId } }))?.notified ?? false;
	},
	async createRegistrationCode(player) {
		const code = Math.floor(100000 + Math.random() * 900000).toString();
		await db.player.upsert({
			where: { eosId: player.eosId },
			create: { eosId: player.eosId, steamName: player.steamName, code },
			update: { code },
		});
		return code;
	},
	async markNotified(eosId) {
		await db.player.update({ where: { eosId }, data: { notified: true } });
	},
	async getUndeliveredRewards(eosId) {
		return db.pendingReward.findMany({ where: { eosId, delivered: false } });
	},
	async markRewardDelivered(eosId, blueprint) {
		await db.pendingReward.updateMany({ where: { eosId, blueprint }, data: { delivered: true } });
	},
};

const stop = startRegistrationPoller(api, repo, {
	intervalMs: 20_000,
	discordInviteUrl: "https://discord.gg/your-invite",
	onError: (err) => console.error("[ArkRegistrationPoller]", err),
});

// stop() when your panel shuts down, if you want a clean exit.
```

## The Discord bot side (not included here)

When your bot handles `/registrar <code>`, it needs to:
1. Look up the code in the same table `createRegistrationCode` wrote to.
2. If valid (and not expired — add a `codeCreatedAt` column and check an expiry
   window, e.g. 15 minutes, so old codes can't be reused), link the Discord user ID to
   that `eosId` and set `registered = true`.
3. Insert one row per welcome item into your `pendingReward` table (blueprint + quantity).
   The poller's next tick delivers them automatically once the player is online — no need
   for the bot to call the ARK API directly, though it can if you'd rather deliver
   immediately when the player happens to already be online (call
   `api.giveItem({ eosId }, blueprint, { quantity })` directly from the bot instead of
   queuing, then skip inserting into `pendingReward`).

## Notes

- `blueprint` strings for `giveItem`/`giveEngrams` must be the full in-game blueprint path,
  e.g. `Blueprint'/Game/PrimalEarth/CoreBlueprints/Items/Weapons/PrimalItem_WeaponRifle.PrimalItem_WeaponRifle_C'`.
- If a player disconnects between getting notified and registering, `wasNotified` staying
  `true` means they won't be spammed again on reconnect — they'll just see the reward
  delivered (or nothing, if they never registered) on their next poll cycle. If you want a
  "remind me again" behavior, add a TTL to the notified flag instead of a permanent one.
- The poller calls `GET /api/v1/players` on an interval — it does **not** need a webhook or
  any plugin change, since it already lists everyone currently online.
