Import("env")
import subprocess, os

# Try to get the short git hash; if that fails (e.g. no .git), use fallback
try:
    # Get the current Git commit hash (short form)
    commit = subprocess.check_output(
        ["git", "rev-parse", "--short", "HEAD"]
    ).strip().decode("utf-8")
except Exception:
    commit = "Initial"

# Define the macro GIT_COMMIT_HASH with the hash (StringifyMacro adds quotes)
env.Append(CPPDEFINES=[("GIT_COMMIT_HASH", env.StringifyMacro(commit))])
