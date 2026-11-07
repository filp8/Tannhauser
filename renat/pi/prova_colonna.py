#!/usr/bin/env python3
"""Prova automatica di una colonna: homing, misura e cicli di movimento.

Uso:
    python3 prova_colonna.py               # 1 ciclo
    python3 prova_colonna.py --cicli 20    # prova di durata
    python3 prova_colonna.py --log         # mostra anche i comandi scambiati

Ogni ciclo: va a 25%, 75%, 50% della corsa e torna a 0, poi controlla
dove scatta il finecorsa MIN (deve essere intorno a -200: se si sposta,
il motore ha perso passi) e rifa' l'homing.
Ctrl+C ferma il motore e termina.
"""

import argparse
import time

from octopus import Octopus, OctopusError


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cicli", type=int, default=1)
    parser.add_argument("--log", action="store_true")
    args = parser.parse_args()

    with Octopus(log=args.log) as oct:
        print(oct.info())
        try:
            print("Homing...")
            oct.home()
            print("Misura della corsa...")
            max_trigger, utile = oct.misura()
            print(f"  corsa utile = {utile} passi (MAX scatta a {max_trigger})")
            oct.home()

            derive = []
            for n in range(1, args.cicli + 1):
                t0 = time.monotonic()
                for frazione in (0.25, 0.75, 0.5, 0.0):
                    oct.goto(round(utile * frazione))
                scatto = oct.verifica_zero()
                derive.append(scatto + 200)
                oct.home()
                print(f"Ciclo {n}/{args.cicli}: {time.monotonic() - t0:.0f} s, "
                      f"MIN scattato a {scatto} (scarto {scatto + 200:+d} passi)")

            print(f"\nCicli completati: {len(derive)}. "
                  f"Scarto MIN: min {min(derive):+d}, max {max(derive):+d} passi")
            oct.abilita(False)

        except KeyboardInterrupt:
            print("\nInterrotto: fermo il motore.")
            oct.stop()
            oct.abilita(False)
        except OctopusError as e:
            print(f"\nERRORE dalla Octopus: {e}")
            try:
                oct.stop()
                oct.abilita(False)
            except OctopusError:
                pass
            raise SystemExit(1)


if __name__ == "__main__":
    main()
