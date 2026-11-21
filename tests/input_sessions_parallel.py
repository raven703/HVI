"""Two real Runtime processes consume the same Slave-facing API independently."""
import concurrent.futures
import pathlib
import subprocess
import sys

exe = pathlib.Path(sys.argv[1]).resolve()


def run(session):
    result = subprocess.run([str(exe), str(session)], cwd=exe.parent,
                            capture_output=True, text=True, timeout=15)
    assert result.returncode == 0, result.stdout + result.stderr
    return result.stdout


with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    print("".join(pool.map(run, [123, 124])))