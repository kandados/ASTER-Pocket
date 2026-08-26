from pathlib import Path
from array import array
import base64
import sys
import time
import wave

import serial


PORT = "/dev/cu.usbmodem2401"
BAUD = 115200
RATE = 24000

desktop = Path.home() / "Desktop"

raw_path = desktop / "ASTER-Pocket-voz-raw.wav"
normalized_path = desktop / "ASTER-Pocket-voz-normalizada.wav"


def write_wav(path, pcm):
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(RATE)
        wav.writeframes(pcm)


print("[Mac] Abriendo A.S.T.E.R. Pocket...")

with serial.Serial(
    PORT,
    BAUD,
    timeout=1
) as ser:

    time.sleep(0.5)
    ser.reset_input_buffer()

    print()
    print("[Mac] Solicitando grabación...")
    print("[Mac] Mira los mensajes y habla cuando aparezca HABLA AHORA.")
    print()

    ser.write(b"r\n")
    ser.flush()

    dumping = False
    dump_requested = False
    base64_parts = []

    deadline = time.time() + 120

    while time.time() < deadline:
        raw_line = ser.readline()

        if not raw_line:
            continue

        line = raw_line.decode(
            "utf-8",
            errors="replace"
        ).strip()

        if not dumping:
            print(line)

        if (
            line.startswith("[AsterVoice]") and
            line.endswith("Grabación OK.") and
            not dump_requested
        ):
            dump_requested = True

            print()
            print("[Mac] Grabación terminada.")
            print("[Mac] Descargando PCM desde Pocket...")
            print()

            ser.write(b"d\n")
            ser.flush()

        elif line.startswith(
            "[AsterVoiceDump] BEGIN"
        ):
            dumping = True

        elif line == "[AsterVoiceDump] END":
            break

        elif dumping:
            if line:
                base64_parts.append(
                    line
                )

    else:
        raise RuntimeError(
            "Timeout esperando la grabación."
        )


if not base64_parts:
    raise RuntimeError(
        "No se recibió audio desde Pocket."
    )


print("[Mac] Decodificando PCM...")

pcm = base64.b64decode(
    "".join(base64_parts),
    validate=True
)

print(
    f"[Mac] PCM recibido: {len(pcm)} bytes"
)

write_wav(
    raw_path,
    pcm
)


samples = array("h")
samples.frombytes(pcm)

if sys.byteorder == "big":
    samples.byteswap()


peak = max(
    abs(sample)
    for sample in samples
)

if peak == 0:
    gain = 1.0
else:
    gain = min(
        24000.0 / peak,
        256.0
    )


normalized = array(
    "h",
    (
        max(
            -32768,
            min(
                32767,
                int(sample * gain)
            )
        )
        for sample in samples
    )
)


output_samples = normalized

if sys.byteorder == "big":
    output_samples.byteswap()


write_wav(
    normalized_path,
    output_samples.tobytes()
)


print()
print("[Mac] PRUEBA COMPLETADA")
print(
    f"[Mac] Pico original: {peak}"
)
print(
    f"[Mac] Ganancia de escucha: x{gain:.2f}"
)
print()
print(
    f"[Mac] WAV original: {raw_path}"
)
print(
    f"[Mac] WAV normalizado: {normalized_path}"
)
