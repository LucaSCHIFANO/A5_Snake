set_xmakever("2.6.1")

set_project("Snake")

add_rules("mode.asan", "mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

set_allowedmodes("debug", "releasedbg", "asan")
set_allowedplats("windows", "mingw", "linux", "macosx")
set_allowedarchs("windows|x64", "mingw|x86_64", "linux|x86_64", "macosx|x86_64")
set_defaultmode("debug")

-- On configure la SFML sans son module réseau 😇
add_requires("sfml~client", {alias = "sfml_client", configs = { network = false, main = false }})
add_requires("sfml~server", {alias = "sfml_server", configs = { audio = false, graphics = false, window = false, network = false, main = false }})

-- Petite configuration de base
add_sysincludedirs("thirdparty/include")
add_syslinks("ws2_32")

set_languages("c89", "cxx17")
set_rundir("./bin")
set_symbols("debug", "hidden")
set_targetdir("./bin/$(plat)_$(arch)_$(mode)")
set_warnings("allextra")

if is_plat("windows") then
	set_runtimes(is_mode("debug") and "MDd" or "MD")

	add_defines("_CRT_SECURE_NO_WARNINGS")
	add_cxxflags("/permissive-", "/Zc:__cplusplus", "/Zc:externConstexpr", "/Zc:inline", "/Zc:lambda", "/Zc:preprocessor", "/Zc:referenceBinding", "/Zc:strictStrings", "/Zc:throwingNew")
	add_cxflags("/w44062") -- Enable warning: switch case not handled
end

target("SnakeGame")
   set_kind("binary")

   add_headerfiles("cl_**.hpp", "sh_**.hpp")
   add_files("cl_**.cpp", "sh_**.cpp")
   add_packages("sfml_client")

target("SnakeServer")
   set_kind("binary")

   add_headerfiles("sv_**.hpp", "sh_**.hpp")
   add_files("sv_**.cpp", "sh_**.cpp")
   add_packages("sfml_server")
