# Psychiatry: Unknown 0.2.0 — Patch Notes

Release date: 2026-11-27

This update turns the original mood/trauma expansion into a full psychiatry
layer: new phobias with distinct triggers, new psychoactive liquids, and fixes
for several triggers that never fired in 0.1.x.

## Highlights

- Nine new phobias: acrophobia, pharmacophobia, narcophobia, tsibophobia,
  ablutophobia, necrophobia, splanchnophobia, aquaphobia, petrophobia.
- Six new craftable liquids: Venlafaxine, Propranolol, Hydroxyzine, Pregabalin,
  MDMA, Psilocybin.
- Renamed and fixed Humanoidophobia → Anthropomorphophobia and
  Bushophobia → Botanophobia.
- Compatibility with Unknown Drugs: `diazepam` now works as a benzodiazepine
  for panic suppression.
- Worked around a native-loader vector marshalling bug that silently broke
  every position- and velocity-based trigger.

## Added

### Phobias

| Phobia | Trigger |
|---|---|
| Acrophobia | long falls, especially with fractures |
| Pharmacophobia | overdose on medicines |
| Narcophobia | overdose on narcotics |
| Tsibophobia | repeated overeating |
| Ablutophobia | staying very dirty for a long time |
| Necrophobia | seeing too many corpses |
| Splanchnophobia | seeing `internalorgans` |
| Aquaphobia | spending a long time in water |
| Petrophobia | spending a long time in oil |

Each phobia has its own moodle, exposure counter, and in-game effect.

### Liquids and recipes

All new liquids are craftable:

- **Venlafaxine** — gradually reduces all phobias while active.
- **Propranolol** — suppresses the physical symptoms of fear: tachycardia,
  tremors, adrenaline spikes. Does not cure the phobias themselves.
- **Hydroxyzine** — sedating anxiolytic; helps prevent panic attacks.
- **Pregabalin** — gradually dulls phobias and anxiety.
- **MDMA** — instantly removes all phobias, then causes a severe bad trip:
  nausea, vomiting, hyperthermia, hallucinations, mood swings, and serotonin
  load.
- **Psilocybin** — instantly removes all phobias, then causes a heavy trip:
  hallucinations, screen distortion, nausea, and panic.

Recipes are listed in `README.md`.

### Compatibility

- Unknown Drugs: liquid `diazepam` is recognized as a benzodiazepine and
  suppresses panic attacks the same way `psy_benzo` does.

## Changed

- **Humanoidophobia → Anthropomorphophobia.** The description and moodle were
  renamed to match the intended meaning.
- **Bushophobia → Botanophobia.** Same for the bush-related phobia.
- Phobia exposure for anthropomorphophobia now comes from damaging and killing
  traders/guards, not only from being attacked by guards.

## Fixed

- **Anthropomorphobia did not trigger when shooting traders.** It now hooks
  `BuildingEntity.set_health` and counts damage and kills of both traders and
  guards.
- **Botanophobia could not be obtained.** The old mine/`BushCol` proximity
  check depended on broken position data. It now uses `BushCol` trigger
  contacts plus a player collision check on the mine: walk through a bush, step
  on a mine, get the phobia.
- **Aquaphobia, petrophobia, and acrophobia never fired.** They were rebuilt on
  scalar signals:
  - water/oil from `Body.inWater` plus `dirtyness` growth;
  - falls from airborne time, the fall scream, and fractures.
- **Native vector marshalling bug workaround.** Unknown Polymorphism returns
  `Vector2`/`Vector3`/`Color` values as `NIL` because `ValueCodec.TryWriteVector`
  takes its output struct by value. All logic that relied on `transform.position`,
  `rb.velocity`, or coordinate-based `GetLiquid` calls was replaced with hooks
  and scalar/object reads.
- **Drinking and injection.** The mod now fully replicates vanilla
  `WaterContainerItem.Drink`/`Inject`, including liquid delegates, condition
  updates, sounds, and injection sickness. Partial drinks keep the remaining
  liquid instead of emptying the container.
- **Item startup NREs.** Fixed hook handling so `Item.Awake`/`Item.Start` run
  normally; previously spawned items could lose `rb`/`affect` and throw
  `NullReferenceException` every frame.
- **Recipes, locale, and item registration** are re-applied after world
  (re)generation, so the new liquids and the needle survive scene reloads.

## Known limitations

- Psychiatric state is kept in memory and is not saved: conditions and phobias
  reset on save load.
- Multiplayer (Together) has not been tested; the mod targets the local player.
- Balance values (timers, chances, thresholds) are empirical and may change.
- The mod hooks several game methods; conflicts with other mods patching the
  same methods are possible.
