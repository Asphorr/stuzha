# Сверка снимков до байта: перед переделкой — `python build/cmp.py save` (эталон
# текущей сборкой в build/ref/), после — `python build/cmp.py [1,b,1i,...]`.
# «i» в конце — с --intro (заставка поверх). Цифры «кадр N мс» не сравниваются.
# Ключи: --noavx, --threads N — передаются игре; --rgb — без четвёртого байта;
# --exe X — снимать не stuzha.exe, а build/X (эталон старой сборкой).
import os, sys, subprocess
import numpy as np

BLD = os.path.dirname(os.path.abspath(__file__))
REF = os.path.join(BLD, "ref")
SHOTS = list("1234567890wbz") + ["1i", "bi", "6i"]
MX0, MX1, MY0, MY1 = 520, 600, 22, 38          # «кадр N мс» справа вверху
EXE = sys.argv[sys.argv.index("--exe") + 1] if "--exe" in sys.argv else "stuzha.exe"

def load(p):
    b = open(p, "rb").read()
    return np.frombuffer(b[54:], dtype=np.uint8).reshape(360, 640, 4)[::-1]

def shoot(s, extra):
    cmd = [os.path.join(BLD, EXE), "--shot" + s[0]] + (["--intro"] if s.endswith("i") else []) + extra
    subprocess.run(cmd, cwd=BLD, check=True)
    return os.path.join(BLD, f"shot{s[0]}.bmp")

def main():
    args = sys.argv[1:]
    extra = []
    if "--noavx" in args: extra.append("--noavx")
    if "--threads" in args: extra += ["--threads", args[args.index("--threads") + 1]]
    rgb = "--rgb" in args
    pos = [a for i, a in enumerate(args) if not a.startswith("--") and not a.isdigit()
           and not (i and args[i - 1] == "--exe")]
    if pos and pos[0] == "save":
        os.makedirs(REF, exist_ok=True)
        for s in SHOTS:
            with open(shoot(s, extra), "rb") as f, open(os.path.join(REF, f"shot{s}.bmp"), "wb") as g:
                g.write(f.read())
        print(f"saved {len(SHOTS)} reference shots to {REF}")
        return
    shots = pos[0].split(",") if pos else SHOTS
    bad = 0
    for s in shots:
        a = load(os.path.join(REF, f"shot{s}.bmp")).astype(int)
        b = load(shoot(s, extra)).astype(int)
        d = np.abs(a - b)
        d = (d[:, :, :3] if rgb else d).max(axis=2)
        d[MY0:MY1, MX0:MX1] = 0
        n = int((d > 0).sum())
        if n:
            bad += 1
            ys, xs = np.nonzero(d)
            print(f"shot{s}: {n} px differ, max {d.max()}, bbox x {xs.min()}..{xs.max()} y {ys.min()}..{ys.max()}")
        else:
            print(f"shot{s}: identical")
    print("ALL IDENTICAL" if bad == 0 else f"{bad} SHOTS DIFFER")
    sys.exit(1 if bad else 0)

main()
