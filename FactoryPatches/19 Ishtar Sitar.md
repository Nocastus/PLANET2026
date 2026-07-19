# Ishtar Sitar

A sitar. Not a shitar

---

## Amplitude Envelope
ampEnvAttackTime = 0.01  (0.001 to 10.0)
ampEnvDecayTime = 2.28  (0.001 to 10.0)
ampEnvSustainLevel = 0.02  (0.000 to 1.0)
ampEnvReleaseTime = 3.00  (0.001 to 10.0)
exponentialControl = 0.70  (0.000 to 1.0)

## Velocity Response
velToAmplitude = 120.00  (0.000 to 200.0)
velToAttackTime = 30.00  (0.000 to 100.0)
vintageAmount = 34.60  (0.000 to 100.0)

## Colour
brilliance = 0.21  (0.000 to 1.0)
carrierMorph = 0.00  (0.000 to 1.0)
brillianceModWheel = 1  (0.000 to 2.0)
carrierMorphModWheel = 0  (0.000 to 2.0)

## Vibrato
vibratoRate = 5.50  (0.500 to 12.0)
vibratoDepth = 0.00  (0.000 to 2.0)
vibratoFadeIn = 2.00  (0.000 to 10.0)

## Pitch Envelope
pitchEnvDistance = 0.00  (-12.000 to 12.0)
pitchEnvTime = 0.50  (0.010 to 5.0)

## Effects
detuneAmount = 0.15  (0.000 to 1.0)
detuneMix = 0.30  (0.000 to 1.0)
warmth = 0.20  (0.000 to 1.0)
punch = 0.50  (0.000 to 1.0)
punchFrequency = 1800  (500.000 to 5000.0)

## Portamento
portamentoTime = 0.00  (0.000 to 5.0)
portamentoMode = 0  (0.000 to 1.0)

## Unison
unisonVoices = 1  (1.000 to 4.0)
unisonDetune = 12.00  (0.000 to 50.0)

## Output
patchTrim = 0.00  (-12.000 to 12.0)


---

## Drawbar Parameters Reference
# k = -2.0 to 2.0
# EnvelopeAmount = -5.0 to 20.0
# LFOAmount = -5.0 to 5.0
# input_f = 0.5 to 30.0 (0.5 steps)
# AttackTime = 0.001 to 10.0
# DecayTime = 0.001 to 10.0
# SustainLevel = 0.0 to 2.0
# ReleaseTime = 0.001 to 10.0
# LFOShape = 1, 2, or 3 (Sine, Triangle, Square)
# LFORate = 0.05 to 1000.0
# LFOSync = 0 (Free) or 1 (Tempo Sync)
# LFOSyncDiv = 0-12 (4/1, 2/1, 1/1, 1/2., 1/2, 1/2T, 1/4., 1/4, 1/4T, 1/8., 1/8, 1/8T, 1/16)

# VelToHarmonic = -100 to 100
# ToPM = 0 (off) or 1 (on) - route drawbar to phase-distortion path
# ToOut = 0 (off) or 1 (on) - route drawbar direct to output (additive partial)
# TrigSingle = 0 (Multi/retrigger) or 1 (Single/phrase-start) - single-trigger envelope (Hammond perc)
# Density = 0.0 to 1.0 - modulator morph sine -> soft-saw (experimental)
# Noise = 0 (off) or 1 (on) - drawbar becomes band-pass noise centred on harmonic F; Density = width (experimental)
## Drawbar 1
k1 = 0.80  (-2.000 to 2.0)
k1EnvelopeAmount = 0.50  (-5.000 to 20.0)
k1LFOAmount = 0.00  (-5.000 to 5.0)
k1VelToHarmonic = 0.00  (-100.000 to 100.0)
input_f1 = 1.00  (0.500 to 30.0)
k1AttackTime = 0.01  (0.001 to 10.0)
k1DecayTime = 0.50  (0.001 to 10.0)
k1SustainLevel = 0.30  (0.000 to 2.0)
k1ReleaseTime = 2.00  (0.001 to 10.0)
k1LFOShape = 1  (1.000 to 4.0)
k1LFORate = 0.30  (0.050 to 1000.0)
k1LFOSync = 0  (0.000 to 1.0)
k1LFOSyncDiv = 7  (0.000 to 12.0)
k1ToPM = 1  (0.000 to 1.0)
k1ToOut = 0  (0.000 to 1.0)
k1TrigSingle = 0  (0.000 to 1.0)
k1Density = 0.00  (0.000 to 1.0)
k1Noise = 0  (0.000 to 1.0)

## Drawbar 2
k2 = 0.60  (-2.000 to 2.0)
k2EnvelopeAmount = 1.20  (-5.000 to 20.0)
k2LFOAmount = 0.05  (-5.000 to 5.0)
k2VelToHarmonic = 0.00  (-100.000 to 100.0)
input_f2 = 2.00  (0.500 to 30.0)
k2AttackTime = 0.08  (0.001 to 10.0)
k2DecayTime = 0.80  (0.001 to 10.0)
k2SustainLevel = 0.20  (0.000 to 2.0)
k2ReleaseTime = 2.50  (0.001 to 10.0)
k2LFOShape = 1  (1.000 to 4.0)
k2LFORate = 0.25  (0.050 to 1000.0)
k2LFOSync = 0  (0.000 to 1.0)
k2LFOSyncDiv = 7  (0.000 to 12.0)
k2ToPM = 1  (0.000 to 1.0)
k2ToOut = 0  (0.000 to 1.0)
k2TrigSingle = 0  (0.000 to 1.0)
k2Density = 0.00  (0.000 to 1.0)
k2Noise = 0  (0.000 to 1.0)

## Drawbar 3
k3 = 0.45  (-2.000 to 2.0)
k3EnvelopeAmount = 2.00  (-5.000 to 20.0)
k3LFOAmount = 0.10  (-5.000 to 5.0)
k3VelToHarmonic = 0.00  (-100.000 to 100.0)
input_f3 = 3.00  (0.500 to 30.0)
k3AttackTime = 0.15  (0.001 to 10.0)
k3DecayTime = 1.00  (0.001 to 10.0)
k3SustainLevel = 0.15  (0.000 to 2.0)
k3ReleaseTime = 3.00  (0.001 to 10.0)
k3LFOShape = 1  (1.000 to 4.0)
k3LFORate = 0.20  (0.050 to 1000.0)
k3LFOSync = 0  (0.000 to 1.0)
k3LFOSyncDiv = 7  (0.000 to 12.0)
k3ToPM = 1  (0.000 to 1.0)
k3ToOut = 0  (0.000 to 1.0)
k3TrigSingle = 0  (0.000 to 1.0)
k3Density = 0.00  (0.000 to 1.0)
k3Noise = 0  (0.000 to 1.0)

## Drawbar 4
k4 = 0.30  (-2.000 to 2.0)
k4EnvelopeAmount = 3.00  (-5.000 to 20.0)
k4LFOAmount = 0.15  (-5.000 to 5.0)
k4VelToHarmonic = 0.00  (-100.000 to 100.0)
input_f4 = 4.00  (0.500 to 30.0)
k4AttackTime = 0.25  (0.001 to 10.0)
k4DecayTime = 1.50  (0.001 to 10.0)
k4SustainLevel = 0.10  (0.000 to 2.0)
k4ReleaseTime = 3.50  (0.001 to 10.0)
k4LFOShape = 1  (1.000 to 4.0)
k4LFORate = 0.18  (0.050 to 1000.0)
k4LFOSync = 0  (0.000 to 1.0)
k4LFOSyncDiv = 7  (0.000 to 12.0)
k4ToPM = 1  (0.000 to 1.0)
k4ToOut = 0  (0.000 to 1.0)
k4TrigSingle = 0  (0.000 to 1.0)
k4Density = 0.00  (0.000 to 1.0)
k4Noise = 0  (0.000 to 1.0)

## Drawbar 5
k5 = 0.25  (-2.000 to 2.0)
k5EnvelopeAmount = 4.00  (-5.000 to 20.0)
k5LFOAmount = 0.20  (-5.000 to 5.0)
k5VelToHarmonic = 0.00  (-100.000 to 100.0)
input_f5 = 5.00  (0.500 to 30.0)
k5AttackTime = 0.40  (0.001 to 10.0)
k5DecayTime = 2.00  (0.001 to 10.0)
k5SustainLevel = 0.08  (0.000 to 2.0)
k5ReleaseTime = 4.00  (0.001 to 10.0)
k5LFOShape = 1  (1.000 to 4.0)
k5LFORate = 0.15  (0.050 to 1000.0)
k5LFOSync = 0  (0.000 to 1.0)
k5LFOSyncDiv = 7  (0.000 to 12.0)
k5ToPM = 1  (0.000 to 1.0)
k5ToOut = 0  (0.000 to 1.0)
k5TrigSingle = 0  (0.000 to 1.0)
k5Density = 0.00  (0.000 to 1.0)
k5Noise = 0  (0.000 to 1.0)

## Drawbar 6
k6 = 0.20  (-2.000 to 2.0)
k6EnvelopeAmount = 5.00  (-5.000 to 20.0)
k6LFOAmount = 0.25  (-5.000 to 5.0)
k6VelToHarmonic = 0.00  (-100.000 to 100.0)
input_f6 = 6.00  (0.500 to 30.0)
k6AttackTime = 0.60  (0.001 to 10.0)
k6DecayTime = 2.50  (0.001 to 10.0)
k6SustainLevel = 0.05  (0.000 to 2.0)
k6ReleaseTime = 4.50  (0.001 to 10.0)
k6LFOShape = 1  (1.000 to 4.0)
k6LFORate = 0.12  (0.050 to 1000.0)
k6LFOSync = 0  (0.000 to 1.0)
k6LFOSyncDiv = 7  (0.000 to 12.0)
k6ToPM = 1  (0.000 to 1.0)
k6ToOut = 0  (0.000 to 1.0)
k6TrigSingle = 0  (0.000 to 1.0)
k6Density = 0.00  (0.000 to 1.0)
k6Noise = 0  (0.000 to 1.0)

## Drawbar 7
k7 = 0.15  (-2.000 to 2.0)
k7EnvelopeAmount = 6.00  (-5.000 to 20.0)
k7LFOAmount = 0.30  (-5.000 to 5.0)
k7VelToHarmonic = 0.00  (-100.000 to 100.0)
input_f7 = 7.00  (0.500 to 30.0)
k7AttackTime = 0.80  (0.001 to 10.0)
k7DecayTime = 3.00  (0.001 to 10.0)
k7SustainLevel = 0.03  (0.000 to 2.0)
k7ReleaseTime = 5.00  (0.001 to 10.0)
k7LFOShape = 1  (1.000 to 4.0)
k7LFORate = 0.10  (0.050 to 1000.0)
k7LFOSync = 0  (0.000 to 1.0)
k7LFOSyncDiv = 7  (0.000 to 12.0)
k7ToPM = 1  (0.000 to 1.0)
k7ToOut = 0  (0.000 to 1.0)
k7TrigSingle = 0  (0.000 to 1.0)
k7Density = 0.00  (0.000 to 1.0)
k7Noise = 0  (0.000 to 1.0)

## Drawbar 8
k8 = 0.10  (-2.000 to 2.0)
k8EnvelopeAmount = 7.00  (-5.000 to 20.0)
k8LFOAmount = 0.35  (-5.000 to 5.0)
k8VelToHarmonic = 0.00  (-100.000 to 100.0)
input_f8 = 8.00  (0.500 to 30.0)
k8AttackTime = 1.00  (0.001 to 10.0)
k8DecayTime = 3.50  (0.001 to 10.0)
k8SustainLevel = 0.02  (0.000 to 2.0)
k8ReleaseTime = 5.50  (0.001 to 10.0)
k8LFOShape = 1  (1.000 to 4.0)
k8LFORate = 0.08  (0.050 to 1000.0)
k8LFOSync = 0  (0.000 to 1.0)
k8LFOSyncDiv = 7  (0.000 to 12.0)
k8ToPM = 1  (0.000 to 1.0)
k8ToOut = 0  (0.000 to 1.0)
k8TrigSingle = 0  (0.000 to 1.0)
k8Density = 0.00  (0.000 to 1.0)
k8Noise = 0  (0.000 to 1.0)

## Drawbar 9
k9 = 0.08  (-2.000 to 2.0)
k9EnvelopeAmount = 8.00  (-5.000 to 20.0)
k9LFOAmount = 0.40  (-5.000 to 5.0)
k9VelToHarmonic = 0.00  (-100.000 to 100.0)
input_f9 = 9.00  (0.500 to 30.0)
k9AttackTime = 1.20  (0.001 to 10.0)
k9DecayTime = 4.00  (0.001 to 10.0)
k9SustainLevel = 0.01  (0.000 to 2.0)
k9ReleaseTime = 6.00  (0.001 to 10.0)
k9LFOShape = 1  (1.000 to 4.0)
k9LFORate = 0.06  (0.050 to 1000.0)
k9LFOSync = 0  (0.000 to 1.0)
k9LFOSyncDiv = 7  (0.000 to 12.0)
k9ToPM = 1  (0.000 to 1.0)
k9ToOut = 0  (0.000 to 1.0)
k9TrigSingle = 0  (0.000 to 1.0)
k9Density = 0.00  (0.000 to 1.0)
k9Noise = 0  (0.000 to 1.0)

## Drawbar 10
k10 = 0.05  (-2.000 to 2.0)
k10EnvelopeAmount = 9.00  (-5.000 to 20.0)
k10LFOAmount = 0.45  (-5.000 to 5.0)
k10VelToHarmonic = 0.00  (-100.000 to 100.0)
input_f10 = 10.00  (0.500 to 30.0)
k10AttackTime = 1.50  (0.001 to 10.0)
k10DecayTime = 4.50  (0.001 to 10.0)
k10SustainLevel = 0.00  (0.000 to 2.0)
k10ReleaseTime = 6.50  (0.001 to 10.0)
k10LFOShape = 1  (1.000 to 4.0)
k10LFORate = 0.05  (0.050 to 1000.0)
k10LFOSync = 0  (0.000 to 1.0)
k10LFOSyncDiv = 7  (0.000 to 12.0)
k10ToPM = 1  (0.000 to 1.0)
k10ToOut = 0  (0.000 to 1.0)
k10TrigSingle = 0  (0.000 to 1.0)
k10Density = 0.00  (0.000 to 1.0)
k10Noise = 0  (0.000 to 1.0)

