# analyze_acc_simple.py
# Simple analyzer for ESP32 MMA8451 CSV logs:
# Columns: timedelta_ms, Xacc, Yacc, Zacc

import numpy as np
import matplotlib.pyplot as plt

# ========= EDIT THIS =========
CSV_FILE = "log.csv"   # <-- put your filename here
MAX_FREQ_HZ = 50       # spectrum x-axis limit (set to None for full Nyquist)
# =============================


def load_csv(path):
    """
    Loads the CSV with a header row:
    timedelta_ms,Xacc,Yacc,Zacc
    Returns: t_ms, ax, ay, az (all numpy arrays)
    """
    data = np.genfromtxt(path, delimiter=",", names=True, dtype=float)

    t_ms = data["timedelta_ms"]
    ax = data["Xacc"]
    ay = data["Yacc"]
    az = data["Zacc"]

    # Remove any rows with missing values
    ok = np.isfinite(t_ms) & np.isfinite(ax) & np.isfinite(ay) & np.isfinite(az)
    return t_ms[ok], ax[ok], ay[ok], az[ok]


def estimate_fs_from_timedelta_ms(t_ms):
    """
    Estimates sample rate from the time column.
    Uses median dt for robustness.
    """
    dt_ms = np.diff(t_ms)
    dt_ms = dt_ms[np.isfinite(dt_ms) & (dt_ms > 0)]
    if dt_ms.size < 2:
        raise ValueError("Not enough valid time steps to estimate sampling rate.")
    median_dt_ms = np.median(dt_ms)
    fs_hz = 1000.0 / median_dt_ms
    return fs_hz, median_dt_ms


def fft_spectrum(signal, fs_hz):
    """
    Computes one-sided FFT amplitude spectrum (rough amplitude).
    No preprocessing is applied (signal is not altered).
    """
    x = np.asarray(signal, dtype=float)
    x = x[np.isfinite(x)]
    n = x.size
    if n < 8:
        raise ValueError("Not enough samples for FFT.")

    # Window to reduce leakage (doesn't modify your stored axis values)
    w = np.hanning(n)
    xw = x * w

    X = np.fft.rfft(xw)
    f = np.fft.rfftfreq(n, d=1.0 / fs_hz)

    # amplitude-ish scaling (one-sided)
    scale = np.sum(w) / n
    mag = (np.abs(X) / n) * (2.0 / scale)

    return f, mag


def main():
    t_ms, ax, ay, az = load_csv(CSV_FILE)

    amag = np.sqrt(ax * ax + ay * ay + az * az)

    fs_hz, median_dt_ms = estimate_fs_from_timedelta_ms(t_ms)
    print(f"File: {CSV_FILE}")
    print(f"Samples: {t_ms.size}")
    print(f"Estimated fs: {fs_hz:.3f} Hz (median dt = {median_dt_ms:.3f} ms, Nyquist = {fs_hz/2:.3f} Hz)")

    # --- Time plot ---
    plt.figure()
    plt.title("Acceleration vs Time")
    plt.plot(t_ms / 1000.0, ax, label="Xacc")
    plt.plot(t_ms / 1000.0, ay, label="Yacc")
    plt.plot(t_ms / 1000.0, az, label="Zacc")
    plt.plot(t_ms / 1000.0, amag, label="AccMag", linewidth=2.0)
    plt.xlabel("Time (s)")
    plt.ylabel("Acceleration (m/s²)")
    plt.grid(True, alpha=0.3)
    plt.legend()

    # --- FFT spectra ---
    fx, mx = fft_spectrum(ax, fs_hz)
    fy, my = fft_spectrum(ay, fs_hz)
    fz, mz = fft_spectrum(az, fs_hz)
    fm, mm = fft_spectrum(amag, fs_hz)

    plt.figure()
    plt.title("FFT Spectrum (X, Y, Z, Magnitude)")
    plt.plot(fx, mx, label="Xacc")
    plt.plot(fy, my, label="Yacc")
    plt.plot(fz, mz, label="Zacc")
    plt.plot(fm, mm, label="AccMag", linewidth=2.0)
    plt.xlabel("Frequency (Hz)")
    plt.ylabel("Amplitude (approx)")
    plt.grid(True, alpha=0.3)
    plt.legend()

    if MAX_FREQ_HZ is not None:
        plt.xlim(0, MAX_FREQ_HZ)

    plt.show()


if __name__ == "__main__":
    main()
