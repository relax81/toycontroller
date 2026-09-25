# PlatformIO pre-script: embed the short git hash (with -dirty if there are
# uncommitted changes) as GIT_HASH, shown in the [fw] line at startup.
import subprocess
Import("env")

try:
    rev = subprocess.check_output(
        ["git", "describe", "--always", "--dirty", "--abbrev=7"],
        cwd=env.subst("$PROJECT_DIR"), stderr=subprocess.DEVNULL
    ).decode().strip()
except Exception:
    rev = "unknown"

env.Append(CPPDEFINES=[("GIT_HASH", env.StringifyMacro(rev))])
