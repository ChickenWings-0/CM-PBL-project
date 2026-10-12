"""
extract_uci_day.py  --  one-time data preparation (NOT part of the C program)

Extracts one complete day from the UCI "Individual Household Electric Power
Consumption" dataset and writes the three small CSV files the C program reads.

Usage:
    python scripts/extract_uci_day.py <path to household_power_consumption.txt>

Output (in data/):
    real_day_minute.csv          1441 rows, 00:00 .. 24:00, every minute
    real_day_hourly_complete.csv 25 rows, the minute reading at every hh:00
    real_day_hourly.csv          same 25 rows, hours 4, 9, 13, 14, 19 set to NA

See docs/data_source.md for the citation and the reasoning behind each step.
"""
import os
import sys

DAY = "14/5/2007"            # day to extract (format used in the UCI file: d/m/yyyy)
NEXT_DAY = "15/5/2007"       # needed for the 24:00 reading (= 00:00 of next day)
MISSING_HOURS = {4, 9, 13, 14, 19}   # same pattern as the synthetic dataset

HERE = os.path.dirname(os.path.abspath(__file__))
DATA_DIR = os.path.join(HERE, "..", "data")


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(1)

    # Step 1: read only the rows of DAY and the first row of NEXT_DAY.
    # Columns: Date;Time;Global_active_power;...  ('?' marks a missing value)
    minute = {}
    next_midnight = None
    with open(sys.argv[1]) as f:
        next(f)                                   # skip header
        for line in f:
            parts = line.strip().split(";")
            date, time, gap = parts[0], parts[1], parts[2]
            if date == DAY:
                minute[time] = gap
            elif date == NEXT_DAY and time == "00:00:00":
                next_midnight = gap
                break

    # Step 2: check the day is complete (all 1440 minutes, no '?').
    times = ["%02d:%02d:00" % (h, m) for h in range(24) for m in range(60)]
    missing = [t for t in times if minute.get(t, "?") == "?"]
    if missing or next_midnight in (None, "?"):
        sys.exit("Day %s is not complete (%d minutes missing)" % (DAY, len(missing)))

    values = [float(minute[t]) for t in times] + [float(next_midnight)]  # 1441 values

    # Step 3: write the 1-minute reference file (time in decimal hours).
    with open(os.path.join(DATA_DIR, "real_day_minute.csv"), "w", newline="\n") as f:
        f.write("# UCI Individual Household Electric Power Consumption, %s, 1-minute data\n" % DAY)
        f.write("# Global_active_power = household minute-averaged active power (kW)\n")
        f.write("time_hours,power_kW\n")
        for k, v in enumerate(values):
            f.write("%.6f,%.3f\n" % (k / 60.0, v))

    # Step 4: hourly readings = the minute value at each hh:00 (instantaneous sample).
    hourly = [values[60 * h] for h in range(25)]
    with open(os.path.join(DATA_DIR, "real_day_hourly_complete.csv"), "w", newline="\n") as f:
        f.write("# UCI household, %s: minute reading at every hh:00 (ground truth)\n" % DAY)
        f.write("time_hours,power_kW\n")
        for h, v in enumerate(hourly):
            f.write("%d,%.3f\n" % (h, v))

    # Step 5: same readings with some hours deliberately removed (simulated meter dropouts).
    with open(os.path.join(DATA_DIR, "real_day_hourly.csv"), "w", newline="\n") as f:
        f.write("# UCI household, %s: hourly smart-meter readings\n" % DAY)
        f.write("# Hours 4, 9, 13, 14, 19 removed on purpose (NA) to simulate lost readings\n")
        f.write("time_hours,power_kW\n")
        for h, v in enumerate(hourly):
            f.write("%d,%s\n" % (h, "NA" if h in MISSING_HOURS else "%.3f" % v))

    print("Metered energy for %s: %.4f kWh" % (DAY, sum(values[:1440]) / 60.0))
    print("Files written to", os.path.normpath(DATA_DIR))


if __name__ == "__main__":
    main()
