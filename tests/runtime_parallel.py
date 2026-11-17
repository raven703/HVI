import subprocess, pathlib, concurrent.futures
root=pathlib.Path(__file__).resolve().parents[1]
def run(id):
    p=subprocess.run([str(root/'build/runtime_tests.exe'),str(id)],capture_output=True,text=True,timeout=12,cwd=root/'build')
    assert p.returncode==0,p.stdout+p.stderr
    return p.stdout
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    print(''.join(pool.map(run,[81,82])))