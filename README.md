# Compact Incubator for the EvolChip Bioreactor

This repository accompanies a Bachelor's thesis at TU Berlin titled *Development of a Temperature Control System*, in collaboration with the Charité — Universitätsmedizin Berlin. The work is part of the GlobalResist project, which studies the development of bacterial antibiotic resistance using the EvolChip microfluidic bioreactor.

The thesis covers the design, construction, and experimental characterisation of a compact 3D-printed incubator that maintains the EvolChip at 37 °C during microscope-based experiments. The system uses a PTC ceramic heater with an integrated fan, two DS18B20 temperature sensors, a solid state relay, and an Arduino-based three-phase on/off controller with duty-cycled warm-up.

## Repository Contents

- **`arduino/`** — Arduino source code for the temperature controller, including the three-phase control logic (warmup, hold, cool), sensor acquisition, safety threshold, and serial logging.
- **`data/`** — Raw datasets from the experimental runs reported in the thesis, including sensor calibration, initial heating response, start-up repeatability, 24-hour steady-state holding, and the cooling response.

## Author

Prince Jacob
TU Berlin — Computational Engineering Science
2026
