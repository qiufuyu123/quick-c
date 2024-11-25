add_rules("mode.debug", "mode.release")

target("quick-c")
    set_kind("binary")
    add_files("src/*.c")
    add_includedirs("include")
    add_defines("BOOT_C")
    if is_mode("debug") then
        add_defines("DEBUG")
    end