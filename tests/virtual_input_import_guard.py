"""Architecture guard: core input path cannot request OS input or focus/cursor changes.

Observational desktop tests cannot attribute external user activity to HVI.
Inspect imports instead; this is not a proof against dynamic API resolution.
"""
import pathlib
import re
import subprocess
import sys

objdump = sys.argv[1]
for name in sys.argv[2:]:
    path = pathlib.Path(name)
    imports = subprocess.check_output([objdump, "-p", str(path)], text=True)
    forbidden = re.findall(
        r"\b(?:PostMessage[AW]|SendMessage[AW]|SendInput|keybd_event|mouse_event|"
        r"SetCursorPos|SetPhysicalCursorPos|SetForegroundWindow|SetActiveWindow|"
        r"SetFocus|AttachThreadInput|SetCapture|ClipCursor|SetWindowsHookEx[AW])\b",
        imports)
    assert not forbidden, (str(path), forbidden)
    print("PASS no OS input/focus/cursor setters in", path.name)