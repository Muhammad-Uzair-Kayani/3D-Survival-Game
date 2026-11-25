# Frostworks frozen policy evaluation

Checkpoint SHA-256 `c02c5983a1f6ccfb44f950cd57fbdb19fa50b8df50387fa06e92fe3387c0a7e8`.

The paired comparison ran the rule planner and the frozen native model on seeds 9721, 9829, 9931 in each difficulty. Each mission ran for at most 36,060 physics frames, about 601 simulated seconds. Both controllers used fixed baseline survivor profiles.

| Difficulty | Planner extracted | Neural extracted | Planner deaths | Neural deaths | Planner mean extraction time | Neural mean extraction time |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Peaceful | 15/15 | 15/15 | 0 | 0 | 278.8 s | 196.5 s |
| Standard | 3/15 | 0/15 | 0 | 0 | 498.7 s | — |
| Hostile | 0/15 | 5/15 | 1 | 2 | — | 438.0 s |
| Overall | 18/45 | 20/45 | 1 | 2 | 315.4 s | 256.9 s |

Mean extraction time uses the survivors who boarded by the cutoff. [Per-seed results](samples/frostworks-policy-evaluation.json)

In Standard seed 9931, the planner restored the radio and extracted three survivors. The neural team restored power and had all five survivors alive at the cutoff. Its radio was still offline.

## Training and coverage

The imitation pass fitted 34,993 ranked decision groups from nine game episodes and targeted fixtures. The traces exposed all 13 tactical choices. The mission traces and expanded fixture varied 138 of the 144 feature columns. The Python and C++ parity test perturbed all 144 columns independently and in combination.

The full live-training round updated its candidate on 28,396 sampled decisions. The validation gate kept the starting checkpoint because the candidate did not improve the validation score. The supplemental pass fitted previously constant inputs using 163 decision groups. The released checkpoint includes updates to order, order_time, shortcut, target_trust, target_contested, i_smg, i_rifle, i_shotgun, i_shells, i_tools. [Initial training](samples/frostworks-policy-training.json) · [Live round](samples/frostworks-policy-live-round.json) · [Feature coverage](samples/frostworks-policy-coverage.json) · [Supplemental validation](samples/frostworks-policy-calibration.json)

## Windows package verification

The Windows x64 ZIP launched under Wine. Its packaged executable ran all three difficulty smoke missions with the release checkpoint and recorded neural choices at each of the three stages. The combined live launcher collected and fitted 1,441 sampled choices and evaluated its candidate while the game was open. [Windows test record](samples/frostworks-policy-windows.json)
