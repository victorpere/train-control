# Proposed Design: Smooth DC Track Supply

Status: proposed, not built. Replaces the PWM track drive (L298N #1, pins 5 and 6) with a DAC-controlled linear supply and a relay for polarity. The turnout drive (L298N #2) and sensors are unchanged. See [system-setup.md](system-setup.md) for the current setup.

## 1. Goal

Supply the track with steady DC instead of PWM, to avoid the brush arcing and noise that PWM drive can add, while keeping the same serial protocol and speed ramping.

## 2. Design summary

- An MCP4725 DAC (0–5 V, I2C) sets the speed.
- An LM358 op-amp with gain of 2 drives a TIP120 Darlington as an emitter follower, giving 0 to about 8 V.
- A current-limit transistor (Q2) with a 1.2 Ω sense resistor limits the output to about 0.5 A.
- A DPDT relay reverses the track polarity. It switches only when the output is 0 V.
- The Arduino controls the DAC over I2C (pins 20 and 21) and the relay from pin 8.

```mermaid
flowchart LR
    PSU["12-15 V supply"] --> FUSE["Fuse (1 A)"]
    FUSE --> V12["+12 V"]
    MEGA["Arduino Mega 2560"] -- "I2C: pins 20, 21" --> DAC["MCP4725 DAC<br/>0-5 V"]
    DAC --> OPAMP["LM358<br/>gain x2"]
    V12 --> OPAMP
    OPAMP --> Q1["TIP120 follower<br/>+ heatsink"]
    V12 --> Q1
    Q1 --> RS["1.2 ohm sense<br/>+ Q2 current limit"]
    RS --> VREG["VREG: 0-8 V"]
    VREG --> RELAY["DPDT relay K1<br/>polarity"]
    MEGA -- "pin 8 via Q3" --> RELAY
    RELAY --> TRK["Track rails A / B"]
    VREG -. "feedback" .-> OPAMP
```

## 3. Parts list

| Qty | Part | Notes |
|---|---|---|
| 1 | MCP4725 breakout board | 12-bit I2C DAC. Check the I2C address (0x60–0x62). |
| 1 | LM358 (DIP-8) + socket | Dual op-amp, input range includes 0 V |
| 1 | TIP120 (TO-220) | Darlington NPN power transistor |
| 1 | TO-220 heatsink (≤ 6 °C/W) + insulating pad and shoulder washer | The TIP120 tab is the collector at +12 V |
| 2 | 2N2222 / PN2222 (TO-92) | Q2 current limit, Q3 relay driver |
| 1 | 5 V DPDT signal relay (for example Omron G5V-2) | Contacts rated ≥ 1 A |
| 1 | 1N4007 or 1N4148 diode | Relay flyback |
| 2 | 10 kΩ ¼ W resistor (1% preferred) | Gain network |
| 1 | 470 Ω ¼ W resistor | Q1 base |
| 2 | 1 kΩ ¼ W resistor | Output bleed, relay driver base |
| 1 | 1.2 Ω 1 W resistor | Current sense |
| 1 | 100 pF ceramic capacitor | Loop stability |
| 2 | 100 nF ceramic capacitor | Decoupling |
| 1 | 100 µF 25 V electrolytic capacitor | Supply bulk |
| 1 | 12 V 2 A DC supply | Reuse the current supply if rated ≥ 1 A |
| 1 | 1 A fuse or polyfuse | +12 V input |
| – | Perfboard or breadboard, terminal blocks, wire | |

## 4. Connections

All grounds are common (Arduino, supply, and every GND below).

**MCP4725**
- VCC → Mega 5 V, GND → GND, SDA → pin 20, SCL → pin 21
- OUT → LM358 pin 3

**LM358 (U1)**
- Pin 8 → +12 V, with a 100 nF capacitor to GND
- Pin 4 → GND
- Pin 3 (+input A) ← DAC OUT
- Pin 2 (−input A) ← feedback node
- Pin 1 (output A) → 470 Ω → Q1 base
- Unused amplifier B: pin 5 → GND, pin 6 → pin 7

**Feedback (gain of 2)**
- 10 kΩ (Rf) from VREG to LM358 pin 2
- 10 kΩ (Rg) from LM358 pin 2 to GND
- 100 pF in parallel with Rf

**Power stage**
- Q1 (TIP120): collector → +12 V, base ← 470 Ω, emitter → 1.2 Ω resistor
- The other end of the 1.2 Ω resistor is VREG.
- 100 µF from +12 V to GND, close to Q1
- 1 kΩ bleed resistor from VREG to GND

**Current limit (Q2)**
- Base → Q1 emitter (Q1 side of the 1.2 Ω resistor)
- Emitter → VREG
- Collector → Q1 base

**Relay (K1) and driver (Q3)**
- Coil + → Mega 5 V, coil − → Q3 collector
- Q3 emitter → GND, Q3 base ← 1 kΩ ← Mega pin 8
- 1N4007 across the coil (cathode to 5 V, anode to Q3 collector)
- NC_A → VREG, NO_A → GND, NC_B → GND, NO_B → VREG
- COM_A → track rail A, COM_B → track rail B

The relay de-energised gives rail A = +V, rail B = GND. Energised swaps them.

## 5. Schematic sketch

```
                    +12V ──┬───────────────┬───────────┐
                           │              [100µF]      │
                           │               │           │
                           │              GND          C
                           │                      Q1 TIP120
 Mega 5V ─ MCP4725         │            ┌──470Ω──── B
   SDA(20) ─ SDA           │            │           E
   SCL(21) ─ SCL           │            │           │
              OUT ───┐     │            │           ├──────── Q2 base
                     │   ┌─8────┐       │          [1.2Ω]
                     └──3+      1──────┘           │
                       │ LM358  │         Q2 emitter┴── VREG ──┬─ [1kΩ] ─ GND
                  ┌────2−      │      (Q2 collector → Q1 base)  │
                  │    └─4──────┘                                │
                  │      │                                      │
                  │     GND                                     │
                  ├──[10k Rg]── GND                             │
                  └──[10k Rf ‖ 100pF]───────────────────────────┤
                                                                │
   Mega pin 8 ─[1k]─ Q3 base                                    │
   Q3 collector ── relay coil ── Mega 5V   (1N4007 across coil) │
   Q3 emitter ── GND                                            │
                                                                │
   Relay K1 (DPDT)                                              │
     NC_A ──────────────────────────────────────────────────── VREG
     NO_A ── GND
     NC_B ── GND
     NO_B ──────────────────────────────────────────────────── VREG
     COM_A ── Track rail A
     COM_B ── Track rail B
```

## 6. Detailed block diagram

Full system with the new track supply. L298N #1 is no longer used for the track and can be kept as a spare.

```mermaid
flowchart TB
    subgraph POWER["Power"]
        PSU["12-15 V DC supply"]
        FUSE["Fuse / polyfuse 1 A"]
        V12["+12 V terminal block"]
        GNDB["GND terminal block<br/>(star point)"]
        PSU -- "+" --> FUSE --> V12
        PSU -- "-" --> GNDB
    end

    subgraph MCU["Arduino Mega 2560"]
        P20["Pin 20 SDA"]
        P21["Pin 21 SCL"]
        P8["Pin 8 relay"]
        P22["Pins 22-25 turnouts"]
        PS["Pins 2, 3, 4, 7 sensors"]
        P5V["5 V"]
        PUSB["USB serial (0/1)"]
    end

    PC["Host PC"]
    PUSB <--> PC

    subgraph SUPPLY["Smooth DC track supply (new)"]
        DAC["MCP4725 DAC<br/>0-5 V"]
        U1["U1 LM358<br/>gain x2"]
        Q1["Q1 TIP120<br/>emitter follower<br/>+ heatsink"]
        RS["Rs 1.2 ohm"]
        Q2["Q2 current limit<br/>about 0.5 A"]
        VREG["VREG 0-8 V<br/>+ 1 kohm bleed"]
        Q3["Q3 relay driver"]
        K1["K1 DPDT relay<br/>polarity"]
        DAC --> U1 --> Q1 --> RS --> VREG
        Q2 -. "limits Q1 base" .-> Q1
        RS -. "sense" .-> Q2
        VREG -. "feedback Rf/Rg" .-> U1
        VREG --> K1
        Q3 --> K1
    end

    subgraph TURN["Turnout drive (unchanged)"]
        L2["L298N #2<br/>ENA/ENB jumpered"]
    end

    subgraph SENS["Sensors (unchanged)"]
        S0["A3144 S0"]
        S1["A3144 S1"]
        S2["A3144 S2"]
        S3["A3144 S3"]
    end

    TRK["Track rails A / B"]
    T0["Turnout 0"]
    T1["Turnout 1"]

    V12 --> U1
    V12 --> Q1
    V12 --> L2
    GNDB --> MCU
    GNDB --> SUPPLY
    GNDB --> L2
    GNDB --> SENS

    P20 --> DAC
    P21 --> DAC
    P5V --> DAC
    P8 --> Q3
    P5V --> K1
    P22 --> L2
    P5V --> SENS
    SENS --> PS

    K1 --> TRK
    L2 -- "OUT1/OUT2" --> T0
    L2 -- "OUT3/OUT4" --> T1
```

## 7. Pin map

### 7.1 Arduino Mega 2560

| Arduino pin | Function | Connected to | Label | Status |
|---|---|---|---|---|
| 0 / 1 | USB serial (9600 baud) | Host PC | – | Unchanged |
| 2 | Sensor 0 (`INPUT_PULLUP`, active low) | A3144 #0 signal | `S0` | Unchanged |
| 3 | Sensor 1 | A3144 #1 signal | `S1` | Unchanged |
| 4 | Sensor 2 | A3144 #2 signal | `S2` | Unchanged |
| 7 | Sensor 3 | A3144 #3 signal | `S3` | Unchanged |
| 5 | Former track forward PWM | – | – | Freed, unused |
| 6 | Former track reverse PWM | – | – | Freed, unused |
| 8 | Relay direction (via Q3) | Q3 base through 1 kΩ | `REL` | New |
| 20 | I2C SDA | MCP4725 SDA | `SDA` | New |
| 21 | I2C SCL | MCP4725 SCL | `SCL` | New |
| 22 | Turnout 0, side A | L298N #2 IN1 | `T0-A` | Unchanged |
| 23 | Turnout 0, side B | L298N #2 IN2 | `T0-B` | Unchanged |
| 24 | Turnout 1, side A | L298N #2 IN3 | `T1-A` | Unchanged |
| 25 | Turnout 1, side B | L298N #2 IN4 | `T1-B` | Unchanged |
| 5 V | Supply for DAC, relay coil and sensors | See wiring table | – | Extended |
| GND | Common ground | GND terminal block | – | Unchanged |

### 7.2 MCP4725 DAC

| Module pin | Connected to |
|---|---|
| VCC | Mega 5 V |
| GND | GND |
| SDA | Mega pin 20 |
| SCL | Mega pin 21 |
| OUT | U1 pin 3 |
| A0 | GND (default address; see board documentation) |

### 7.3 LM358 (U1, DIP-8)

| Pin | Name | Connected to |
|---|---|---|
| 1 | OUT A | 470 Ω → Q1 base |
| 2 | −IN A | Feedback node (Rf, Rg, Cf) |
| 3 | +IN A | DAC OUT |
| 4 | GND | GND |
| 5 | +IN B | GND (unused amplifier) |
| 6 | −IN B | Pin 7 (unused amplifier) |
| 7 | OUT B | Pin 6 (unused amplifier) |
| 8 | V+ | +12 V (100 nF to GND) |

### 7.4 Transistors (check each datasheet for pinout)

| Part | Terminal | Connected to |
|---|---|---|
| Q1 TIP120 | Base | 470 Ω from U1 pin 1; Q2 collector |
| Q1 TIP120 | Collector (tab) | +12 V |
| Q1 TIP120 | Emitter | Rs (1.2 Ω) and Q2 base |
| Q2 2N2222 | Base | Q1 emitter |
| Q2 2N2222 | Emitter | VREG |
| Q2 2N2222 | Collector | Q1 base |
| Q3 2N2222 | Base | 1 kΩ from Mega pin 8 |
| Q3 2N2222 | Collector | K1 coil − |
| Q3 2N2222 | Emitter | GND |

### 7.5 Relay K1 (DPDT, 5 V coil)

| Terminal | Connected to |
|---|---|
| Coil + | Mega 5 V |
| Coil − | Q3 collector (1N4007 across the coil, cathode to 5 V) |
| Pole A: COM | Track rail A |
| Pole A: NC | VREG |
| Pole A: NO | GND |
| Pole B: COM | Track rail B |
| Pole B: NC | GND |
| Pole B: NO | VREG |

## 8. Wiring table

Net names: `+12V`, `GND`, `+5V`, `DAC_OUT`, `FB` (feedback node), `Q1B` (Q1 base), `Q1E` (Q1 emitter, before Rs), `VREG` (after Rs), `Q3C` (Q3 collector), `TRK_A`, `TRK_B`. Suggested colours follow the scheme in [system-setup.md](system-setup.md): red +12 V, black GND, orange +5 V, blue sensor, white logic, yellow/green track.

| # | From | To | Net | Colour |
|---|---|---|---|---|
| 1 | 12 V supply + | Fuse, then +12 V terminal block | `+12V` | Red |
| 2 | 12 V supply − | GND terminal block | `GND` | Black |
| 3 | +12 V terminal block | U1 pin 8 | `+12V` | Red |
| 4 | +12 V terminal block | Q1 collector | `+12V` | Red |
| 5 | +12 V terminal block | L298N #2 +12 V | `+12V` | Red |
| 6 | 100 µF capacitor + | +12 V, near Q1 | `+12V` | – |
| 7 | 100 µF capacitor − | GND | `GND` | – |
| 8 | 100 nF capacitor | U1 pin 8 to GND | `+12V` / `GND` | – |
| 9 | GND terminal block | Arduino GND | `GND` | Black |
| 10 | GND terminal block | MCP4725 GND | `GND` | Black |
| 11 | GND terminal block | U1 pin 4 | `GND` | Black |
| 12 | GND terminal block | U1 pin 5 | `GND` | Black |
| 13 | GND terminal block | Rg (10 kΩ), bottom end | `GND` | Black |
| 14 | GND terminal block | Q3 emitter | `GND` | Black |
| 15 | GND terminal block | Relay NO_A and NC_B | `GND` | Black |
| 16 | GND terminal block | VREG bleed resistor (1 kΩ), bottom end | `GND` | Black |
| 17 | GND terminal block | L298N #2 GND | `GND` | Black |
| 18 | GND terminal block | A3144 GND (all four) | `GND` | Black |
| 19 | Arduino 5 V | MCP4725 VCC | `+5V` | Orange |
| 20 | Arduino 5 V | Relay coil + | `+5V` | Orange |
| 21 | Arduino 5 V | Relay flyback diode cathode | `+5V` | Orange |
| 22 | Arduino 5 V | A3144 VCC (all four) | `+5V` | Orange |
| 23 | Arduino pin 20 | MCP4725 SDA | `SDA` | White |
| 24 | Arduino pin 21 | MCP4725 SCL | `SCL` | White |
| 25 | MCP4725 OUT | U1 pin 3 | `DAC_OUT` | White |
| 26 | U1 pin 6 | U1 pin 7 | – | – |
| 27 | U1 pin 1 | 470 Ω, then Q1 base | `Q1B` | White |
| 28 | Q2 collector | Q1 base | `Q1B` | – |
| 29 | Q1 emitter | Rs (1.2 Ω) and Q2 base | `Q1E` | – |
| 30 | Rs (other end) | Q2 emitter | `VREG` | Yellow |
| 31 | VREG | Rf (10 kΩ) | `VREG` | – |
| 32 | Rf (other end) | U1 pin 2, Rg, and Cf | `FB` | – |
| 33 | Cf (100 pF) | Across Rf | `FB` / `VREG` | – |
| 34 | VREG | Bleed resistor (1 kΩ) top end | `VREG` | – |
| 35 | VREG | Relay NC_A and NO_B | `VREG` | Yellow |
| 36 | Arduino pin 8 | 1 kΩ, then Q3 base | `REL` | White |
| 37 | Q3 collector | Relay coil − | `Q3C` | – |
| 38 | Q3 collector | Flyback diode anode | `Q3C` | – |
| 39 | Relay COM_A | Track rail A | `TRK_A` | Yellow |
| 40 | Relay COM_B | Track rail B | `TRK_B` | Green |
| 41 | Arduino pins 22, 23 | L298N #2 IN1, IN2 | `T0-A`, `T0-B` | White |
| 42 | Arduino pins 24, 25 | L298N #2 IN3, IN4 | `T1-A`, `T1-B` | White |
| 43 | L298N #2 OUT1, OUT2 | Turnout 0 | – | Green |
| 44 | L298N #2 OUT3, OUT4 | Turnout 1 | – | Green |
| 45 | A3144 #0, #1, #2, #3 signal | Arduino pins 2, 3, 4, 7 | `S0`–`S3` | Blue |

Module settings:
- Keep the ENA and ENB jumpers fitted on L298N #2.
- Leave the L298N 5 V terminal unconnected.
- L298N #1 is not wired in this design. Disconnect its IN1/IN2 from pins 5 and 6 and its track output, or remove it.
- Recommended: 10 kΩ pull-downs from each L298N #2 IN line (pins 22–25) to GND, and a 10 kΩ pull-down from pin 8 to GND so the relay stays off during reset.

## 9. Calculations

- **Output voltage:** $V_{out} = 2 \times V_{DAC}$.
- **Maximum output:** About 8.5 V. The LM358 output reaches about 10.5 V on a 12 V supply, minus roughly 1.5 V for the TIP120 base-emitter drop and about 0.4 V across the sense resistor at 0.3 A. Cap the DAC at about 4 V (code ≈ 3276), giving about 8 V.
- **Current limit:** $I_{limit} \approx 0.65\ \text{V} / 1.2\ \Omega \approx 0.5\ \text{A}$.
- **Heat in Q1:** $P = (V_{in} - V_{out}) \times I$.
  - 12 V in, 4 V out, 0.3 A: about 2.4 W.
  - Short circuit at the current limit: about 6.5 W. The heatsink must handle this, and shorts must be cleared quickly.

## 10. Firmware changes (not yet made)

- Replace `analogWrite` on pins 5 and 6 with a DAC write (for example the Adafruit MCP4725 library).
- Map $|voltage|$ (0 to 127) to DAC codes 0 to about 3276.
- Add a direction pin (pin 8). On a direction change: ramp to 0, wait about 100 ms, switch the relay, then ramp up.
- Remove the Timer 3 and Timer 4 PWM frequency changes.
- Consider a minimum start voltage, because smooth DC has weaker stiction breakaway than PWM.
- Keep the serial protocol, ramp rate and turnout and sensor code unchanged.

## 11. Test plan

1. Build the circuit with a 100 Ω load instead of the locomotive.
2. Check that the output is 0 V at DAC code 0, and about twice the DAC voltage in between.
3. Check that the output is stable and does not oscillate (scope if available).
4. Check the current limit with an ammeter and a brief short.
5. Check heatsink temperature at the expected load.
6. Test relay switching with the output at 0 V.
7. Test with a known-good locomotive (Kato 10-1898) before any other.

## 12. Trade-offs and risks

- **Smooth, quiet DC** and exact 0 V, but **inefficient**: the difference between supply and track voltage is dissipated as heat.
- Suited to a single locomotive at modest current. Several locomotives or higher current would need a larger heat budget or a buck-converter design.
- Low-speed crawling may be worse than with PWM, because smooth DC gives no pulse boost for starting.
- Linear feedback with a Darlington follower can oscillate if it is wired badly. Keep leads short, keep the 100 pF and 470 Ω parts, and avoid a large output capacitor.
- The TIP120 tab is live at +12 V, so insulate the heatsink.

## 13. Alternatives

- **L298N as the polarity switch:** Feed VREG into L298N #1's motor supply terminal, drive IN1 and IN2 as plain digital outputs, and keep ENA on. This reuses existing hardware and avoids the relay, but costs about 2 V, so the maximum track voltage falls to about 6 V.
- **Digitally controlled buck converter:** More efficient and better for higher current, but needs a modified feedback network and more careful design.

## 14. Related files

- [system-setup.md](system-setup.md): current setup, pin map and wiring.
