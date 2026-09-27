# PlatformIO pre-script: expose the repo's VERSION file as FIRMWARE_VERSION.
Import("env")
import os

path = os.path.join(env["PROJECT_DIR"], "..", "VERSION")
try:
    version = open(path, encoding="utf-8").read().strip() or "dev"
except OSError:
    version = "dev"
env.Append(CPPDEFINES=[("FIRMWARE_VERSION", env.StringifyMacro(version))])
