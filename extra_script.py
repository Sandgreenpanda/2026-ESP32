# A script I found online to fix my clanged red underlines issue. Ignore it.

import os
Import("env")
env.Replace(COMPILATIONDB_INCLUDE_TOOLCHAIN=True)
env.Replace(COMPILATIONDB_PATH="compile_commands.json")


def refresh_compile_db(target, source, env):
    env.Execute("pio run -t compiledb")


env.AddPreAction("buildprog", refresh_compile_db)
