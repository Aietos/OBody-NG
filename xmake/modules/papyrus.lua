-- Shared helpers for the Papyrus/Pyro step. Used by `xmake papyrus` and `xmake dist`.
import("core.base.json")

local PPJ_FILE    = "obody.ppj"
local CONFIG_FILE = "pyro.config.json"

-- Keep the local, user-edited config out of `git add` (idempotent, harmless if it fails)
function lock_config()
    if os.isdir(".git") then
        try { function () os.runv("git", {"update-index", "--skip-worktree", CONFIG_FILE}) end }
    end
end

-- Compile the scripts: pyro.exe obody.ppj [--game-path ..] [remote options]
-- opt.force_overwrite: re-download remote (GitHub/Bitbucket) imports even if unchanged
function compile(opt)
    opt = opt or {}
    if not os.isfile(CONFIG_FILE) then
        raise("%s not found. Restore it from git and fill in the path to pyro.exe.", CONFIG_FILE)
    end
    lock_config()

    local config = json.loadfile(CONFIG_FILE) or {}
    local pyro = config.pyro_path
    if not pyro or pyro == "" or not os.isfile(pyro) then
        raise("Set \"pyro_path\" in %s to the full path of pyro.exe (current value: '%s').",
              CONFIG_FILE, tostring(pyro or ""))
    end
    if not os.isfile(PPJ_FILE) then
        raise("%s not found in the project root.", PPJ_FILE)
    end

    local argv = {PPJ_FILE}
    if config.game_path and config.game_path ~= "" then
        table.insert(argv, "--game-path")
        table.insert(argv, config.game_path)
    end

    -- Remotes (GitHub/Bitbucket URLs in the .ppj). Tokens are never read from the config file:
    -- use the PYRO_ACCESS_TOKEN environment variable, or leave it unset and put the token in a
    -- `.secrets` file next to pyro.exe, which Pyro reads by itself.
    local token = os.getenv("PYRO_ACCESS_TOKEN")
    if token and token ~= "" then
        table.insert(argv, "--access-token")
        table.insert(argv, token)
    end
    if opt.force_overwrite then
        table.insert(argv, "--force-overwrite")
    end

    cprint("${bright green}>> ${clear}Compiling Papyrus scripts with Pyro")
    os.execv(pyro, argv)
end
