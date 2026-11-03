#!/usr/bin/env python3
"""Test PING/PONG con la Octopus via USB seriale.

Uso:
    python3 test_ping.py            # usa la porta trovata in /dev/serial/by-id/
    python3 test_ping.py <porta>    # oppure indica la porta a mano
"""
import glob
import statistics
import sys
import time

import serial

N_PING = 100


def trova_porta():
    porte = sorted(glob.glob("/dev/serial/by-id/*"))
    if not porte:
        sys.exit("Nessuna porta in /dev/serial/by-id/: la Octopus e' collegata e accesa?")
    if len(porte) > 1:
        print("Porte trovate:", *porte, sep="\n  ")
        print("Uso la prima. Per sceglierne un'altra: python3 test_ping.py <porta>")
    return porte[0]


def main():
    porta = sys.argv[1] if len(sys.argv) > 1 else trova_porta()
    print("Porta:", porta)

    with serial.Serial(porta, 115200, timeout=1) as s:
        time.sleep(0.5)
        s.reset_input_buffer()

        s.write(b"INFO\n")
        print("INFO ->", s.readline().decode(errors="replace").strip())

        tempi = []
        errori = 0
        for i in range(N_PING):
            t0 = time.perf_counter()
            s.write(b"PING\n")
            risposta = s.readline().decode(errors="replace").strip()
            dt_ms = (time.perf_counter() - t0) * 1000
            if risposta == "PONG":
                tempi.append(dt_ms)
            else:
                errori += 1
                print(f"#{i}: risposta inattesa {risposta!r}")

    print(f"PONG ricevuti: {len(tempi)}/{N_PING}, errori: {errori}")
    if tempi:
        print(f"Tempo di risposta: medio {statistics.mean(tempi):.2f} ms, "
              f"massimo {max(tempi):.2f} ms")


if __name__ == "__main__":
    main()
