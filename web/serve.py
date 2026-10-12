"""
serve.py - tiny local server that connects the web page to the C engine.

    python web/serve.py            then open  http://localhost:8000
    python web/serve.py 8080       (another port)

It does two things, using only the Python standard library:
  1. serves the files in web/ (the page, styles, scripts, fonts);
  2. answers /api/... requests by RUNNING energy.exe and returning its JSON.

So every number on the page is computed by the C program. The page can also
be opened directly from disk (web/index.html); it then shows the snapshot in
web/data/results.js (made by "energy.exe --export-web") and inputs are locked.
"""
import http.server
import json
import math
import os
import subprocess
import sys
import tempfile
import urllib.parse

WEB_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(WEB_DIR)                          # project folder
# build.bat makes energy.exe; on Linux/Mac the binary may be called either name
EXE = next((os.path.join(ROOT, name) for name in ("energy.exe", "energy")
            if os.path.exists(os.path.join(ROOT, name))), os.path.join(ROOT, "energy.exe"))


def run_engine(args):
    """Run energy.exe with args (output file appended) and return (status, json_text)."""
    if not os.path.exists(EXE):
        return 503, json.dumps({"error": "energy.exe not found - run build.bat first"})
    fd, out_path = tempfile.mkstemp(suffix=".json")
    os.close(fd)
    try:
        subprocess.run([EXE] + args[:2] + [out_path] + args[2:], cwd=ROOT,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=60)
        with open(out_path, encoding="utf-8") as f:
            text = f.read()
        if not text.strip():
            return 500, json.dumps({"error": "the C engine produced no output"})
        return (400 if text.startswith('{"error"') else 200), text
    except subprocess.TimeoutExpired:
        return 504, json.dumps({"error": "the C engine took too long"})
    finally:
        os.remove(out_path)


def number(params, key, default):
    """Read one numeric query parameter (raises ValueError if it is not a number)."""
    value = float(params.get(key, [str(default)])[0])
    if not math.isfinite(value):                         # reject "nan" and "inf"
        raise ValueError(key)
    return str(value)


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=WEB_DIR, **kwargs)

    def send_json(self, status, text):
        body = text.encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        url = urllib.parse.urlparse(self.path)
        params = urllib.parse.parse_qs(url.query)

        if url.path == "/api/ping":
            return self.send_json(200, json.dumps({"ok": True, "engine": os.path.exists(EXE)}))

        if url.path == "/api/run":
            dataset = params.get("dataset", ["synthetic"])[0]
            if dataset not in ("synthetic", "real"):
                return self.send_json(400, json.dumps({"error": "unknown dataset"}))
            try:
                values = [number(params, "threshold", 3), number(params, "budget", 15),
                          number(params, "peak_start", 18), number(params, "peak_end", 22),
                          number(params, "rate", 8), str(int(float(params.get("fill", ["1"])[0]))),
                          number(params, "probe", -1)]
            except ValueError:
                return self.send_json(400, json.dumps({"error": "every input must be a number"}))
            return self.send_json(*run_engine(["--json", dataset] + values))

        if url.path == "/api/tests":
            return self.send_json(*run_engine(["--json-tests"]))

        return super().do_GET()

    def log_message(self, fmt, *args):            # keep the console quiet except API calls
        msg = str(args[0]) if args else ""
        if "/api/" in msg:
            sys.stderr.write("  %s\n" % (fmt % args))


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
    server = http.server.ThreadingHTTPServer(("127.0.0.1", port), Handler)
    print("Smart Energy web front-end: http://localhost:%d   (Ctrl+C to stop)" % port)
    if not os.path.exists(EXE):
        print("WARNING: %s not found - build it first with build.bat" % EXE)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopped.")


if __name__ == "__main__":
    main()
