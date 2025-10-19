-- set minimum xmake version
set_xmakever("2.8.2")

-- includes
includes ("lib/commonlibsse-ng")

-- set project
set_project("OBody")
set_version("4.4.0")
set_license("GPL-3.0")

-- set defaults
set_languages("c++23")
set_warnings("allextra")

-- set policies
set_policy("package.requires_lock", true)

-- add rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- set default mode to releasedbg
set_defaultmode("releasedbg")

-- require packages
add_requires("rapidjson", "pugixml")
local cfg = {
    configs = {
        baseline = "980c981a8ab8dcf9389c9d10c3e1bc20b6ee23f9",
        default_registry = {
            kind = "git",
            repository = "https://github.com/Microsoft/vcpkg",
            baseline = "980c981a8ab8dcf9389c9d10c3e1bc20b6ee23f9"
        }
    }
}
add_requires("vcpkg::ryml", cfg)
add_requires("vcpkg::boost-algorithm", cfg)

-- targets
target("OBody")
    -- add dependencies to target
    add_deps("commonlibsse-ng")

    -- add packages to target
    add_packages("rapidjson", "pugixml", "vcpkg::ryml", "vcpkg::boost-algorithm")
    add_defines("SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE")

    -- add commonlibsse-ng plugin
    add_rules("commonlibsse-ng.plugin", {
        name = "OBody",
        author = "Aietos"
    })

    -- add MSVC preprocessor flag for __VA_OPT__ support
    if is_plat("windows") then
        add_cxflags("cl::/Zc:preprocessor")
    end

    -- add src files
    add_files("src/**.cpp")
    add_headerfiles("include/**.h", "include/ActorTracker/*.h")
    add_includedirs("include")
    set_pcxxheader("include/PCH.h")

    -- postbuild: copy .dll and .pdb files to contrib/Distribution/data/skse/plugins
    after_build(function(target)
        import("core.project.config")

        config.load()

        -- Helper function to copy a file to multiple directories
        local function copy_file_to_dirs(file, dirs)
            if not os.isfile(file) then return end
            for _, dir in ipairs(dirs) do
                if dir and os.isdir(dir) then
                    local dest = path.join(dir, path.filename(file))
                    os.trycp(file, dest)
                    print("Copied %s to %s", file, dest)
                end
            end
        end

        -- Always copy files after build, don't rely on dependency tracking
        local target_file = target:targetfile()
        local symbol_file = target:symbolfile()
        local dest_dir = "contrib/Distribution/data/skse/plugins"

        -- Optional secondary folder for MO2
        local mo2_dir = os.getenv("MO2_MODS_DIR")
        local secondary_dir
        if mo2_dir then
            secondary_dir = path.join(mo2_dir, target:name() .. " - " .. config.get("mode"),"skse/plugins")
        end
        
        -- ensure destination directory exists
        os.mkdir(dest_dir)
        
        -- Copy both .dll and .pdb to all relevant folders
        copy_file_to_dirs(target_file, {dest_dir, secondary_dir})
        copy_file_to_dirs(symbol_file, {dest_dir, secondary_dir})
    end)