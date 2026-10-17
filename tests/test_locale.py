"""Exercise Qt's real system-language selection without a display or audio server."""
import json
import os
from pathlib import Path
import subprocess
import sys

binary = str(Path(sys.argv[1]).resolve())
cases = [
    ({"LANG": "tr_TR.UTF-8"}, "tr"),
    ({"LANG": "en_US.UTF-8"}, "en"),
    ({"LANG": "de_DE.UTF-8"}, "en"),
    ({"LANG": "C"}, "en"),
    ({"LANG": "en_US.UTF-8", "LC_MESSAGES": "tr_TR.UTF-8"}, "tr"),
    ({"LANG": "tr_TR.UTF-8", "LC_ALL": "en_US.UTF-8"}, "en"),
    ({"LANG": "en_US.UTF-8", "LANGUAGE": "tr:en"}, "tr"),
    ({"LANG": "tr_TR.UTF-8", "LANGUAGE": "de:tr"}, "en"),
]
for settings, expected in cases:
    env = {key: value for key, value in os.environ.items()
           if not key.startswith("LC_") and key not in ("LANG", "LANGUAGE", "DISPLAY", "WAYLAND_DISPLAY")}
    env.update(settings)
    result = subprocess.run([binary, "--locale-info"], env=env, text=True,
                            encoding="utf-8", capture_output=True, check=True, timeout=10)
    info = json.loads(result.stdout)
    assert info["language"] == expected, (settings, info)
    assert info["title"] == ("Ses çıkışı" if expected == "tr" else "Audio output"), info
    assert info["percentage"] == ("%40" if expected == "tr" else "40%"), info
    print(f"PASS {settings}: {info['language']}")
print(f"{len(cases)} language cases passed")
