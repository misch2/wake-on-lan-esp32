Import("env")

import subprocess

try:
    git_hash = (
        subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=env.subst("$PROJECT_DIR"),
            stderr=subprocess.DEVNULL,
        )
        .strip()
        .decode()
    )
except Exception:
    git_hash = "unknown"

print(f"[git_version] GIT_HASH = {git_hash}")
env.Append(CPPDEFINES=[("GIT_HASH", env.StringifyMacro(git_hash))])
