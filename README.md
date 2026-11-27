# Psychiatry: Unknown

Version: **0.2.0**

Native module for **Casualties: Unknown** that expands the mood, trauma, and
brain damage systems. It runs through the **Unknown Polymorphism (UP)**
loader and is written in C++ with no managed assemblies.

The mod adds psychiatric conditions, pharmaceuticals, phobias, and a lobotomy
needle. Moodle icons are reused from vanilla game sprites, so no custom art is
required.

---

## Installation

> **Important:** mod files go into
> `BepInEx\plugins\UnknownPolymorphism\mods\`
>
> Recommended layout — a dedicated folder:
>
> ```
> <Game folder>\BepInEx\plugins\UnknownPolymorphism\mods\PsychiatryUnknown\PsychiatryUnknown.dll
> ```
>
> A flat install also works:
>
> ```
> <Game folder>\BepInEx\plugins\UnknownPolymorphism\mods\PsychiatryUnknown.dll
> ```

Steps:

1. Install **BepInEx 5.4.23.x**.
2. Install the **Unknown Polymorphism** loader (the
   `BepInEx\plugins\UnknownPolymorphism\` folder must exist).
3. Copy `PsychiatryUnknown.dll` into
   `BepInEx\plugins\UnknownPolymorphism\mods\PsychiatryUnknown\`.
4. Restart the game completely.

UP does not support hot reloading native DLLs: after replacing the file, close
and relaunch the game.

### Uninstall

Delete `PsychiatryUnknown.dll` from the mod folder and restart the game.

---

## Requirements

- Casualties: Unknown (full version or demo), Unity 2022.3 Mono build.
- BepInEx 5.4.23.x.
- Unknown Polymorphism.
- Windows x64.
- Building from source: MSVC 2022 (x64) and the UP sources next to the mod.

---

## Features

### 1. Antidepressant side effects

Vanilla antidepressants are treated as SSRIs. They gain a hidden side-effect
meter:

- grows with every dose (`Antidepressants.TakeDose`);
- slowly decays on its own;
- is actively reduced by the mod's pharmaceuticals;
- at high levels periodically triggers **one** random effect instead of all of
  them at once:
  - a sharp short mood drop followed by recovery;
  - punches at blocks around the character;
  - self-harm;
  - brain damage;
  - mood swings;
  - nausea and vomiting;
  - raised or lowered temperature;
  - screen darkening/aberration;
  - auditory hallucinations.

### 2. Pharmaceuticals

All pharmaceuticals are liquids. They are crafted from vanilla antidepressants,
so there is a real trade-off between treatment and side effects.

| Drug | Recipe | INT | Volume |
|---|---|---|---|
| Valproic Acid | antidepressants 100 ml + biochem 20 ml + flammablepowder | 8 | 100 ml |
| Quetiapine | antidepressants 100 ml + soothingcrystalshard + flammablepowder | 10 | 100 ml |
| Benzodiazepine | antidepressants 100 ml + biochem 50 ml + water 50 ml | 9 | 100 ml |
| Haloperidol | antidepressants 100 ml + biochem 30 ml + soothingcrystalshard | 12 | 100 ml |
| Venlafaxine | antidepressants 100 ml + biochem 25 ml + flammablepowder | 11 | 100 ml |
| Propranolol | antidepressants 50 ml + water 50 ml + soothingcrystalshard | 9 | 100 ml |
| Hydroxyzine | antidepressants 50 ml + water 50 ml + flammablepowder | 10 | 100 ml |
| Pregabalin | antidepressants 100 ml + biochem 50 ml + soothingcrystalshard | 12 | 100 ml |
| MDMA | antidepressants 50 ml + biochem 50 ml + turbulentcrystalshard | 14 | 60 ml |
| Psilocybin | musharm + biochem 25 ml + water 50 ml | 14 | 60 ml |

Effects:

- **Valproic Acid** — mood stabilizer. Gently returns mood to neutral, reduces
  trauma, strongly reduces the SSRI side-effect meter, and suppresses random
  side effects.
- **Quetiapine** — antipsychotic. Removes psychosis, panic, and aggression;
  lowers adrenaline; mildly sedating and lowers blood pressure.
- **Benzodiazepine** — fast anxiolytic. Instantly removes fear, trauma, and
  adrenaline, but builds dependence. Without it, withdrawal begins: tremor,
  panic, mood drop. Suppresses panic attacks.
- **Haloperidol** — old antipsychotic. Strongly suppresses schizophrenia, but
  causes tremor, stiffness, and emotional flatness.
- **Venlafaxine** — SNRI. Gradually reduces all phobias while active.
- **Propranolol** — beta blocker. Removes the physical symptoms of fear:
  tachycardia, tremor, and adrenaline spikes. Does not cure phobias.
- **Hydroxyzine** — sedating anxiolytic. Helps prevent panic attacks.
- **Pregabalin** — gradually dulls phobias and anxiety; mildly sedating.
- **MDMA** — instantly erases all phobias, but causes a severe bad trip:
  nausea, vomiting, hyperthermia, hallucinations, mood swings, and serotonin
  load.
- **Psilocybin** — instantly erases all phobias, but causes a heavy trip:
  hallucinations, screen distortion, nausea, and panic.

### 2.1. Unknown Drugs compatibility

If **Unknown Drugs** is installed, its liquid `diazepam` is recognized as a
benzodiazepine: it suppresses panic attacks and reduces trauma the same way
`psy_benzo` does. Other Unknown Drugs liquids keep working through their own
delegates.

### 3. Braingrow and Mindwiper

- **Braingrow** — resets all of the mod's psychiatric meters (conditions,
  phobias, side-effect meter). Its vanilla lethality remains.
- **Mindwiper** — also resets all of the mod's meters; other side effects stay
  vanilla.

### 4. Lobotomy needle

New item: `psy_needle`.

| Item | Recipe | INT |
|---|---|---|
| Lobotomy Needle | scraptube + 2× scrapmetal | 14 |

Using it from the hand or inventory:

- brain health −25…40;
- INT −3;
- clears trauma, OCD, and part of PTSD/schizophrenia;
- applies organic personality disorder;
- head pain and bleeding, ragdoll, `lobotomy` sound;
- 2 uses (item condition −0.5 per use), then the item breaks;
- repeated use has a cooldown.

Works both with LMB while held and through normal item use.

### 5. Conditions

| Condition | Cause | Effect |
|---|---|---|
| OCD | high SSRI load or trauma | periodically demands an action (drop an item, switch hands, use an item); ignoring it gives mood −15 and trauma +4 |
| Derealization | trauma > 60, PTSD/C-PTSD | the character freezes for 1.5–3 s; movement is locked |
| PTSD | killing survivors, seeing corpses | flashbacks: hallucinations, screen flash, mood drop, trauma growth |
| C-PTSD | tick/spider bites, repeated severe injuries | stronger flashbacks and fear of creatures |
| Organic personality disorder | prolonged low brain health, lobotomy | constant mood drain and anger bursts; not treatable with antidepressants, Braingrow resets it |
| Schizophrenia | high SSRI load, brain damage + PTSD/C-PTSD | sounds of explosions, mines, turrets, gunshots, growling behind the back, threat music |
| Panic attacks | trauma, C-PTSD, withdrawal | adrenaline spike, jump and involuntary dash |
| Insomnia | PTSD, high SSRI load, trauma | attempts to sleep are often interrupted; fatigue builds up |
| Arachnophobia | prolonged presence near crawlers/ticks | panic when the creatures approach |
| Insomnophobia | 3 failed attempts to sleep | fear at low energy |
| Hemophobia | prolonged bleeding | panic at the sight of your own blood |
| Anthropomorphophobia | attacks by survivors, shooting and killing traders/guards | panic around people |
| Botanophobia | walking through a bush and then triggering a mine | fear near bushes |
| Acrophobia | falls from great heights, especially with fractures | fear while falling; after landing the character cannot get up for a while |
| Pharmacophobia | overdose on medicines | panic when taking medicines |
| Narcophobia | overdose on narcotics | panic when taking narcotics |
| Tsibophobia | frequent overeating (hunger above normal) | anxiety when overeating |
| Ablutophobia | staying very dirty for a long time | anxiety at high dirtiness |
| Necrophobia | seeing too many corpses | panic near corpses |
| Splanchnophobia | frequently seeing `internalorgans` | panic near internal organs |
| Aquaphobia | spending a long time in water | panic in water |
| Petrophobia | spending a long time in oil | panic in oil |

All active conditions are shown as moodles. Icons are vanilla sprites
(`trauma`, `horrified`, `confused`, `braindamage`, `tired`, etc.); names and
descriptions are custom.

---

## Console quick test

If console/cheats are enabled:

```
spawn antidepressants cursor
spawn psy_needle cursor
```

Pour liquid under yourself (`player` is the player's position):

```
floodfill player 1
floodfill player 3
```

`1` is water (aquaphobia), `3` is oil (petrophobia). For acrophobia, fly up
with `noclip` or teleport high and fall.

Add liquid to the container in your hand:

```
addliquid psy_valproate 100
addliquid psy_quetiapine 100
addliquid psy_benzo 100
addliquid psy_haloperidol 100
addliquid psy_venlafaxine 100
addliquid psy_propranolol 100
addliquid psy_hydroxyzine 100
addliquid psy_pregabalin 100
addliquid psy_mdma 60
addliquid psy_psilocybin 60
```

---

## Limitations

- Psychiatric meters and timers live in mod memory and are **not saved**:
  conditions and phobias reset when a save is loaded.
- Multiplayer (Together) has not been tested; the mod targets the local player.
- Balance values (timers, chances, thresholds) are empirical and may change.
- The mod hooks game methods (`Item.Awake`, `WaterContainerItem.Drink/Inject`,
  `Body.UseItem/UseItemInHand/TakeANap`, `MoodleManager.AddAllMoodles`, and
  others). Conflicts with other mods patching the same methods are possible.
- Tested alongside Together and CUCoreLib.

---

## Building from source

MSVC 2022 is required. `build.bat` expects the `UnknownPolymorphism` folder
with `include\up_api.h` next to the mod:

```
F:\AnotherProjects\mods\
  UnknownPolymorphism\
    include\up_api.h
  PsychiatryUnknown\
    build.bat
    src\...
```

Build:

```bat
build.bat
```

The result is `PsychiatryUnknown.dll` in the project root.

### Source layout

```
src\
  module.cpp        UP entry point and module lifecycle
  psych_state.cpp   conditions, drugs, phobias, effects
  registry.cpp      liquids, items, recipes, localization
  hooks.cpp         Harmony hooks and installation
  up_helpers.*      wrappers over the UP C ABI
  psych.hpp         state and public logic interface
```

---

## License

MIT. See `LICENSE`.
