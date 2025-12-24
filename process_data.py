# analyze_acc_simple.py
# Simple analyzer for ESP32 MMA8451 CSV logs:
# Columns: timedelta_ms, Xacc, Yacc, Zacc

import matplotlib.pyplot as plt
import numpy as np

# ========= EDIT THIS =========
CSV_FILE = "Ecja-u.csv"  # <-- put your filename here
MAX_FREQ_HZ = 50  # spectrum x-axis limit (set to None for full Nyquist)
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


def fft_spectrum(signal, fs_hz, remove_dc=False):
    """
    Computes one-sided FFT amplitude spectrum (rough amplitude).
    No preprocessing is applied (signal is not altered).
    """
    x = np.asarray(signal, dtype=float)
    x = x[np.isfinite(x)]
    if remove_dc and x.size:
        x = x - np.mean(x)
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
    print(
        f"Estimated fs: {fs_hz:.3f} Hz (median dt = {median_dt_ms:.3f} ms, Nyquist = {fs_hz / 2:.3f} Hz)"
    )

    # --- Time plots (one per signal) ---
    signals = [
        ("Xacc", ax),
        ("Yacc", ay),
        ("Zacc", az),
        ("AccMag", amag),
    ]

    t_s = t_ms / 1000.0
    fig_time, axes_time = plt.subplots(len(signals), 1, sharex=True)
    fig_time.suptitle("Acceleration vs Time")
    for axis, (name, data) in zip(axes_time, signals):
        axis.plot(t_s, data)
        axis.set_title(name)
        axis.set_ylabel("Acceleration (m/s^2)")
        axis.grid(True, alpha=0.3)
    axes_time[-1].set_xlabel("Time (s)")

    # --- FFT spectra ---
    fx, mx = fft_spectrum(ax, fs_hz)
    fy, my = fft_spectrum(ay, fs_hz)
    fz, mz = fft_spectrum(az, fs_hz)
    fm, mm = fft_spectrum(amag, fs_hz)
    # skip the first two samples since they skew the results
    spectra = [
        ("Xacc", fx[2:], mx[2:]),
        ("Yacc", fy[2:], my[2:]),
        ("Zacc", fz[2:], mz[2:]),
        ("AccMag", fm[1:], mm[1:]),
    ]

    fig_fft, axes_fft = plt.subplots(len(spectra), 1, sharex=True)
    fig_fft.suptitle("FFT Spectrum")
    for axis, (name, freq, mag) in zip(axes_fft, spectra):
        axis.plot(freq, mag)
        axis.set_title(name)
        axis.set_ylabel("Amplitude (approx)")
        axis.grid(True, alpha=0.3)
        if MAX_FREQ_HZ is not None:
            axis.set_xlim(0, MAX_FREQ_HZ)
    axes_fft[-1].set_xlabel("Frequency (Hz)")

    # # --- FFT spectra (DC removed, log scale) ---
    # fx_d, mx_d = fft_spectrum(ax, fs_hz, remove_dc=True)
    # fy_d, my_d = fft_spectrum(ay, fs_hz, remove_dc=True)
    # fz_d, mz_d = fft_spectrum(az, fs_hz, remove_dc=True)
    # fm_d, mm_d = fft_spectrum(amag, fs_hz, remove_dc=True)

    # spectra_dc = [
    #     ("Xacc", fx_d, mx_d),
    #     ("Yacc", fy_d, my_d),
    #     ("Zacc", fz_d, mz_d),
    #     ("AccMag", fm_d, mm_d),
    # ]

    # eps = np.finfo(float).eps
    # fig_fft_dc, axes_fft_dc = plt.subplots(len(spectra_dc), 1, sharex=True)
    # fig_fft_dc.suptitle("FFT Spectrum (DC Removed, Log Scale)")
    # for axis, (name, freq, mag) in zip(axes_fft_dc, spectra_dc):
    #     axis.semilogy(freq, mag + eps)
    #     axis.set_title(name)
    #     axis.set_ylabel("Amplitude (approx)")
    #     axis.grid(True, alpha=0.3, which="both")
    #     if MAX_FREQ_HZ is not None:
    #         axis.set_xlim(0, MAX_FREQ_HZ)
    # axes_fft_dc[-1].set_xlabel("Frequency (Hz)")

    plt.show()


if __name__ == "__main__":
    main()
