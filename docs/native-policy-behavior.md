# Frostworks AI behavior evaluation

Frozen checkpoint SHA-256 `026876cf6fe62cdfb9c0214478639720f1f911c08a1ee0ea00b9acbc69907643`.

## Response to player fire

In the original reproduction, an armed survivor and an armed raider took a hit from behind, ran away, and fired no shots. The updated simulation redirects attention toward recent damage, checks visibility before identifying an attacker, and reassesses the firing position when a shot is blocked. An unarmed survivor can take a reachable retreat. A bystander can continue mission work when a squadmate fires nearby and the bystander lacks evidence of danger.

| Scripted encounter | Original reproduction | Updated game |
| --- | --- | --- |
| Player shoots an armed survivor from behind | 0 return shots | 4 return shots and player damage |
| Player shoots an armed raider from behind | 0 return shots | 4 return shots and player damage |
| Player shoots an unarmed survivor | Retreats, 0 shots | Retreats, 0 shots |
| Bystander hears squadmate shoot at a raider | Mission progress blocked | Mission progress allowed and no shots fired by bystander |
| Survivor takes environmental damage with no attackers nearby | Damage could trigger combat cover behavior | Mission progress allowed with zero shots or suppression |

The updated response suite passes with the original weights on the repaired engine too. The raider uses the game's C++ controller. In the updated suite, the survivor's choices come from the frozen neural checkpoint. The original reproduction and updated encounter place the player and target in the same firing lane. The updated scenario also isolates bystanders and unrelated raiders.

## Mission results

The nine replayed missions match the original release's seeds and 601-second simulation limit. All five survivors use baseline profiles and run with frozen weights.

| Difficulty | Original extracted | Updated extracted | Original deaths | Updated deaths |
| --- | ---: | ---: | ---: | ---: |
| Peaceful | 15/15 | 15/15 | 0 | 0 |
| Standard | 0/15 | 15/15 | 0 | 0 |
| Hostile | 5/15 | 10/15 | 2 | 5 |
| Overall | 20/45 | 40/45 | 2 | 5 |

A second comparison used one fresh seed per difficulty. Each cell shows survivors extracted, with deaths in parentheses.

| Difficulty and seed | Original game | Repaired game with original weights | Repaired game with retrained weights |
| --- | ---: | ---: | ---: |
| Peaceful 4103 | 5/5 (0) | 5/5 (0) | 5/5 (0) |
| Standard 4139 | 2/5 (0) | 5/5 (0) | 5/5 (0) |
| Hostile 4177 | 3/5 (0) | 4/5 (1) | 4/5 (1) |

The original and retrained weights produced the same extraction and death counts on the three fresh seeds when run in the repaired engine.

In a Standard mission with no player input, a comparison using the same weights on both engines isolated the peek fix. Tactic changes went from 992 to 107, and extractions went from 2/4 to 3/4. The final checkpoint reproduced that result with the player stationary throughout.

## Retraining

The imitation passes fitted 8,704, 4,969, and 3,527 ranked decisions as the simulator was revised. The last pass used one mission per difficulty, the expanded fixture, and targeted reaction traces. 133 of the 144 feature columns varied in those traces. Each reinforcement candidate had to pass the available response scenarios and improve the validation score while preserving the extraction and death score in each difficulty.

| Reinforcement round | Sampled decisions | Validation result |
| --- | ---: | --- |
| 1 | 24,068 | Starting weights kept |
| 2 | 21,413 | Starting weights kept |
| 3 | 23,850 | Starting weights kept |
| 4 | 21,252 | Starting weights kept |
| 5 | 18,934 | Starting weights kept |
| 6 | 18,787 | Starting weights kept |

Rounds 1–3 preceded the exposure fix. Rounds 4–5 preceded the peek fix. Round 6 ran on the final engine.

Across the 12 benchmark missions, no survivor spent more than 1.3 seconds aiming without a shot during a peek or suppression action. Timing and outcome data are in the per-seed record.

## Windows package

The Windows x64 ZIP launched under Wine, passed the five response checks from its packaged PCK, and used all three neural decision stages in the Standard mission. The portable trainer fitted 1,446 decisions and evaluated its candidate. The tests verified the packaged files and the checkpoint hashes.

[Per-seed results, response fixtures, training rounds, and ZIP checksum](samples/frostworks-behavior-evaluation.json)
