-- Local build tasks: `xmake papyrus` and `xmake dist`.
-- Add `includes("xmake/tasks.lua")` to xmake.lua.
--
-- NOTE: xmake script bodies (on_run) can't see file-level `local`s, so every constant
-- lives inside the function or in xmake/modules/papyrus.lua.

add_moduledirs(path.join(os.scriptdir(), "modules"))

-- xmake papyrus: only compile the Papyrus scripts with Pyro -----------------
task("papyrus")
    set_category("plugin")
    set_menu {
        usage = "xmake papyrus",
        description = "Compile the Papyrus scripts with Pyro (settings come from pyro.config.json).",
        options = {
            {"f", "force-overwrite", "k", nil, "Re-download remote imports (GitHub/Bitbucket) even if unchanged."}
        }
    }

    on_run(function ()
        import("core.base.option")
        import("papyrus")
        papyrus.compile({force_overwrite = option.get("force-overwrite")})
    end)
task_end()

-- xmake dist: Pyro + DLL + package + zip (local equivalent of the GitHub CI) --
task("dist")
    set_category("plugin")
    set_menu {
        usage = "xmake dist [options]",
        description = "Build the mod locally: compile Papyrus with Pyro, build the DLL and create the release zip.",
        options = {
            {"k", "skip-papyrus", "k", nil, "Skip the Pyro/Papyrus step and reuse the .pex files already in Data/Scripts."},
            {"d", "skip-dll", "k", nil, "Don't compile the DLL; package the DLL/PDB already in build/ (if any)."},
            {"f", "force-overwrite", "k", nil, "Re-download remote imports (GitHub/Bitbucket) even if unchanged."}
        }
    }

    on_run(function ()
        import("core.base.option")
        import("papyrus")

        local PROJECT_NAME = "OBody"
        local STAGE_DIR    = path.join("contrib", "Package")
        local ARTIFACT_DIR = path.join("contrib", "Artifact")

        local function step(msg) cprint("${bright green}>> ${clear}" .. msg) end

        -- 1. Papyrus
        if not option.get("skip-papyrus") then
            papyrus.compile({force_overwrite = option.get("force-overwrite")})
        end

        -- 2. DLL
        if not option.get("skip-dll") then
            step("Building " .. PROJECT_NAME .. ".dll")
            os.execv(os.programfile(), {"-y", PROJECT_NAME})
        end

        -- 3. Stage
        step("Assembling package in " .. STAGE_DIR)
        os.tryrm(STAGE_DIR)
        os.mkdir(STAGE_DIR)

        if not os.isdir("Data") then raise("Data/ folder not found") end
        for _, entry in ipairs(os.filedirs(path.join("Data", "*"))) do
            os.cp(entry, STAGE_DIR)
        end

        if #os.files(path.join(STAGE_DIR, "Scripts", "*.pex")) == 0 then
            raise("No .pex files in Data/Scripts - compile the scripts first (xmake papyrus)")
        end

        local plugins = path.join(STAGE_DIR, "SKSE", "Plugins")
        os.mkdir(plugins)
        for _, name in ipairs({PROJECT_NAME .. ".dll", PROJECT_NAME .. ".pdb"}) do
            local newest, newest_time
            for _, file in ipairs(os.files(path.join("build", "**", name))) do
                local t = os.mtime(file)
                if not newest_time or t > newest_time then newest, newest_time = file, t end
            end
            if newest then
                if option.get("skip-dll") then
                    cprint("${yellow}Reusing existing %s (not rebuilt)", newest)
                end
                os.cp(newest, plugins)
            elseif option.get("skip-dll") then
                cprint("${bright yellow}warning: ${clear}%s not found in build/, packaging without it", name)
            else
                raise("%s was not produced by the build", name)
            end
        end

        for _, junk in ipairs(os.files(path.join(STAGE_DIR, "**", ".git*"))) do os.rm(junk) end

        -- 4. Zip
        local version = io.readfile("xmake.lua"):match('set_version%("([^"]+)"%)')
        if not version then raise("Could not read the version from xmake.lua") end

        os.mkdir(ARTIFACT_DIR)
        local zipfile = path.absolute(path.join(ARTIFACT_DIR, PROJECT_NAME .. "_" .. version .. ".zip"))
        os.tryrm(zipfile)

        -- bsdtar writes .zip with -a. It ships with Windows 10+ (System32\tar.exe); call that
        -- explicitly so a GNU tar earlier in PATH (e.g. from Git) isn't picked up.
        local tar = "tar"
        if is_host("windows") then
            tar = path.join(os.getenv("SystemRoot") or "C:\\Windows", "System32", "tar.exe")
        end

        local entries = {}
        for _, entry in ipairs(os.filedirs(path.join(STAGE_DIR, "*"))) do
            table.insert(entries, path.filename(entry))
        end
        step("Creating " .. zipfile)
        os.execv(tar, table.join({"-a", "-c", "-f", zipfile, "-C", STAGE_DIR}, entries))

        cprint("${bright green}Done:${clear} %s", zipfile)
    end)
task_end()
