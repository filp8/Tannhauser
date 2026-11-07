"""Comunicazione con la BTT Octopus (firmware octopus_colonna) via USB seriale.

Esempio:
    from octopus import Octopus

    with Octopus() as oct:
        print(oct.info())
        oct.home()
        max_trigger, utile = oct.misura()
        oct.home()
        oct.goto(utile // 2)
        print(oct.status())

Protocollo del firmware: righe di testo terminate da '\\n'.
  - comandi immediati (PING, INFO, STATUS, EN, STOP, INVERTI) -> una risposta
  - comandi di movimento (MOVE, GOTO, HOME, MISURA) -> "ACK ..." subito,
    poi "OK ..." / "CORSA ..." alla fine, oppure "ERR ...".
"""

import glob
import time

import serial

PREFISSO_PORTA = "/dev/serial/by-id/usb-STMicroelectronics*"


class OctopusError(Exception):
    """Errore riportato dal firmware (riga "ERR ...") o di comunicazione."""


def trova_porta():
    porte = sorted(glob.glob(PREFISSO_PORTA))
    if not porte:
        raise OctopusError(
            "Octopus non trovata in /dev/serial/by-id/: e' collegata e accesa?")
    return porte[0]


class Octopus:
    def __init__(self, porta=None, timeout_risposta=2.0, log=False):
        self.porta = porta or trova_porta()
        self.timeout_risposta = timeout_risposta
        self.log = log
        self._pendenti = []          # righe arrivate mentre se ne aspettava un'altra
        self._ser = serial.Serial(self.porta, 115200, timeout=0.1)
        time.sleep(0.3)
        self._ser.reset_input_buffer()

    # -- gestione connessione ------------------------------------------------
    def close(self):
        if self._ser and self._ser.is_open:
            self._ser.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    # -- livello basso --------------------------------------------------------
    def _invia(self, testo):
        if self.log:
            print(">>", testo)
        self._ser.write((testo + "\n").encode())
        self._ser.flush()

    def _leggi_riga(self, scadenza):
        while time.monotonic() < scadenza:
            riga = self._ser.readline().decode(errors="replace").strip()
            if riga:
                if self.log:
                    print("<<", riga)
                return riga
        return None

    def _attendi(self, accetta, timeout):
        """Restituisce la prima riga per cui accetta(riga) e' vera.

        Le righe "ERR ..." fanno sollevare OctopusError; le altre righe non
        attese vengono messe da parte in self._pendenti.
        """
        for i, riga in enumerate(self._pendenti):
            if riga.startswith("ERR "):
                del self._pendenti[i]
                raise OctopusError(riga[4:])
            if accetta(riga):
                del self._pendenti[i]
                return riga

        scadenza = time.monotonic() + timeout
        while True:
            riga = self._leggi_riga(scadenza)
            if riga is None:
                raise OctopusError(f"nessuna risposta entro {timeout:.0f} s")
            if riga.startswith("ERR "):
                raise OctopusError(riga[4:])
            if accetta(riga):
                return riga
            self._pendenti.append(riga)

    def comando(self, testo, accetta, timeout=None):
        self._invia(testo)
        return self._attendi(accetta, timeout or self.timeout_risposta)

    # -- comandi immediati ----------------------------------------------------
    def ping(self):
        return self.comando("PING", lambda r: r == "PONG") == "PONG"

    def info(self):
        return self.comando("INFO", lambda r: r.startswith("octopus_"))

    def status(self):
        """Restituisce lo stato come dizionario, es. {'stato': 'FERMO', 'pos': 0, ...}."""
        riga = self.comando("STATUS", lambda r: r.startswith("STATUS "))
        stato = {}
        for coppia in riga.split()[1:]:
            chiave, _, valore = coppia.partition("=")
            try:
                stato[chiave] = int(valore)
            except ValueError:
                stato[chiave] = valore
        return stato

    def abilita(self, on=True):
        self.comando(f"EN {1 if on else 0}", lambda r: r.startswith("OK EN"))

    def stop(self):
        riga = self.comando("STOP", lambda r: r.startswith("OK STOP"))
        return _leggi_pos(riga)

    # -- movimenti ------------------------------------------------------------
    def _movimento(self, testo, fine, timeout, attendi):
        self.comando(testo, lambda r: r.startswith("ACK"))
        if not attendi:
            return None
        return self.attendi_fine(fine, timeout)

    def attendi_fine(self, fine=("OK",), timeout=120.0):
        """Aspetta la fine di un movimento avviato con attendi=False."""
        try:
            return self._attendi(lambda r: r.startswith(fine), timeout)
        except OctopusError as e:
            if "nessuna risposta" in str(e):
                self.stop()   # timeout: meglio fermare il motore
            raise

    def move(self, passi, attendi=True, timeout=120.0):
        riga = self._movimento(f"MOVE {int(passi)}", ("OK",), timeout, attendi)
        return _leggi_pos(riga) if riga else None

    def goto(self, posizione, attendi=True, timeout=120.0):
        riga = self._movimento(f"GOTO {int(posizione)}", ("OK",), timeout, attendi)
        return _leggi_pos(riga) if riga else None

    def home(self, timeout=180.0):
        self._movimento("HOME", ("OK HOME",), timeout, True)

    def misura(self, timeout=180.0):
        """Dopo home(): va verso MAX. Restituisce (max_trigger, utile)."""
        riga = self._movimento("MISURA", ("CORSA",), timeout, True)
        valori = dict(c.partition("=")[::2] for c in riga.split()[1:])
        return int(valori["max_trigger"]), int(valori["utile"])

    def verifica_zero(self, oltre=300, timeout=60.0):
        """Controllo dei passi persi: da pos 0 va verso MIN finche' scatta.

        Con PASSI_MARGINE = 200 nel firmware, il MIN deve scattare intorno a
        pos=-200. Restituisce la posizione in cui e' scattato. Dopo questa
        prova serve un nuovo home().
        """
        self.goto(0, timeout=timeout)
        try:
            self.move(-(200 + oltre), timeout=timeout)
        except OctopusError as e:
            if "FC_MIN" in str(e):
                return _leggi_pos(str(e))
            raise
        raise OctopusError("il finecorsa MIN non e' scattato")


def _leggi_pos(riga):
    for parte in riga.split():
        if parte.startswith("pos="):
            return int(parte[4:])
    return None
