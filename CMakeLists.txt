cmake_minimum_required(VERSION 3.16)

project(Quake3 C)

# Builds a native Quake III Arena engine for modern Linux.
#
#   quake3     - the game client (SDL2 + OpenGL)
#   q3ded      - the dedicated server
#   qagame<arch>.so, cgame<arch>.so, ui<arch>.so
#              - native game modules for baseq3
#
# The id game data (baseq3/pak0.pk3 ... pak8.pk3) is NOT part of this source
# release, you need a legal copy of Quake III Arena to play.

option(BUILD_CLIENT "Build the quake3 client" ON)
option(BUILD_SERVER "Build the q3ded dedicated server" ON)
option(BUILD_GAME_LIBRARIES "Build the native baseq3 game modules" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# the suffix the engine appends to native module names, see Sys_LoadDll
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
	set(Q3_ARCH x86_64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
	set(Q3_ARCH aarch64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i.86|x86)$")
	set(Q3_ARCH i386)
else()
	message(FATAL_ERROR "Unsupported architecture ${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(CODE ${CMAKE_CURRENT_SOURCE_DIR}/code)

# 2005 era code: keep the compiler from optimising on assumptions it breaks
set(Q3_C_FLAGS
	-pipe
	-fno-strict-aliasing
	-fwrapv
	-fsigned-char
	-fno-common
	-fgnu89-inline
	-Wall
	-Wno-unused-variable
	-Wno-unused-but-set-variable
	-Wno-unused-function
	-Wno-missing-braces
	-Wno-parentheses
	-Wno-char-subscripts
	-Wno-pointer-sign
	-Wno-format-truncation
	-Wno-format-overflow
	-Wno-stringop-truncation
	-Wno-stringop-overflow
	-Wno-misleading-indentation
	-Wno-dangling-else
	-Wno-unknown-pragmas
	-Wno-array-bounds
	-Wno-maybe-uninitialized
	-Wno-unused-value
	-Wno-unused-result
	-Wno-unused-label
	-Wno-comment
	-Wno-return-local-addr
	-Wno-address
	-Wno-enum-conversion
	-Wno-implicit-fallthrough
	-Wno-dangling-pointer
	-Wno-use-after-free
	-Wno-restrict
	-Wno-int-in-bool-context
	-Wno-unused-const-variable
	-Wno-int-to-pointer-cast
	-Werror=pointer-to-int-cast
	-Werror=implicit-function-declaration
	-Werror=int-conversion
	-Werror=incompatible-pointer-types
	-Werror=return-type
)
if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
	list(FILTER Q3_C_FLAGS EXCLUDE REGEX "format-truncation|format-overflow|stringop|return-local-addr|dangling-pointer|use-after-free|unused-but-set|maybe-uninitialized|restrict")
	list(APPEND Q3_C_FLAGS -Wno-unknown-warning-option -Wno-self-assign -Wno-tautological-compare
		-Wno-logical-not-parentheses -Wno-absolute-value -Wno-shift-negative-value)
endif()

# the original x86 JIT is 32 bit only, QVMs run in the bytecode interpreter
set(Q3_DEFINES NO_VM_COMPILED)

# --------------------------------------------------------------------------
# shared engine sources
# --------------------------------------------------------------------------

set(QCOMMON_SOURCES
	${CODE}/qcommon/cm_load.c
	${CODE}/qcommon/cm_patch.c
	${CODE}/qcommon/cm_polylib.c
	${CODE}/qcommon/cm_test.c
	${CODE}/qcommon/cm_trace.c
	${CODE}/qcommon/cmd.c
	${CODE}/qcommon/common.c
	${CODE}/qcommon/cvar.c
	${CODE}/qcommon/files.c
	${CODE}/qcommon/md4.c
	${CODE}/qcommon/msg.c
	${CODE}/qcommon/net_chan.c
	${CODE}/qcommon/huffman.c
	${CODE}/qcommon/unzip.c
	${CODE}/qcommon/vm.c
	${CODE}/qcommon/vm_interpreted.c
	${CODE}/game/q_math.c
	${CODE}/game/q_shared.c
)

set(SERVER_SOURCES
	${CODE}/server/sv_bot.c
	${CODE}/server/sv_ccmds.c
	${CODE}/server/sv_client.c
	${CODE}/server/sv_game.c
	${CODE}/server/sv_init.c
	${CODE}/server/sv_main.c
	${CODE}/server/sv_net_chan.c
	${CODE}/server/sv_snapshot.c
	${CODE}/server/sv_world.c
)

set(BOTLIB_SOURCES
	${CODE}/botlib/be_aas_bspq3.c
	${CODE}/botlib/be_aas_cluster.c
	${CODE}/botlib/be_aas_debug.c
	${CODE}/botlib/be_aas_entity.c
	${CODE}/botlib/be_aas_file.c
	${CODE}/botlib/be_aas_main.c
	${CODE}/botlib/be_aas_move.c
	${CODE}/botlib/be_aas_optimize.c
	${CODE}/botlib/be_aas_reach.c
	${CODE}/botlib/be_aas_route.c
	${CODE}/botlib/be_aas_routealt.c
	${CODE}/botlib/be_aas_sample.c
	${CODE}/botlib/be_ai_char.c
	${CODE}/botlib/be_ai_chat.c
	${CODE}/botlib/be_ai_gen.c
	${CODE}/botlib/be_ai_goal.c
	${CODE}/botlib/be_ai_move.c
	${CODE}/botlib/be_ai_weap.c
	${CODE}/botlib/be_ai_weight.c
	${CODE}/botlib/be_ea.c
	${CODE}/botlib/be_interface.c
	${CODE}/botlib/l_crc.c
	${CODE}/botlib/l_libvar.c
	${CODE}/botlib/l_log.c
	${CODE}/botlib/l_memory.c
	${CODE}/botlib/l_precomp.c
	${CODE}/botlib/l_script.c
	${CODE}/botlib/l_struct.c
)
set_source_files_properties(${BOTLIB_SOURCES} PROPERTIES COMPILE_DEFINITIONS BOTLIB)

set(UNIX_SOURCES
	${CODE}/unix/unix_main.c
	${CODE}/unix/unix_net.c
	${CODE}/unix/unix_shared.c
	${CODE}/unix/linux_common.c
	${CODE}/unix/linux_signals.c
)

# --------------------------------------------------------------------------
# quake3 client
# --------------------------------------------------------------------------

if(BUILD_CLIENT)
	find_package(SDL2 REQUIRED)
	find_package(OpenGL REQUIRED)

	set(CLIENT_SOURCES
		${CODE}/client/cl_cgame.c
		${CODE}/client/cl_cin.c
		${CODE}/client/cl_console.c
		${CODE}/client/cl_input.c
		${CODE}/client/cl_keys.c
		${CODE}/client/cl_main.c
		${CODE}/client/cl_net_chan.c
		${CODE}/client/cl_parse.c
		${CODE}/client/cl_scrn.c
		${CODE}/client/cl_ui.c
		${CODE}/client/snd_adpcm.c
		${CODE}/client/snd_dma.c
		${CODE}/client/snd_mem.c
		${CODE}/client/snd_mix.c
		${CODE}/client/snd_wavelet.c
	)

	set(RENDERER_SOURCES
		${CODE}/renderer/tr_animation.c
		${CODE}/renderer/tr_backend.c
		${CODE}/renderer/tr_bsp.c
		${CODE}/renderer/tr_cmds.c
		${CODE}/renderer/tr_curve.c
		${CODE}/renderer/tr_flares.c
		${CODE}/renderer/tr_font.c
		${CODE}/renderer/tr_image.c
		${CODE}/renderer/tr_init.c
		${CODE}/renderer/tr_light.c
		${CODE}/renderer/tr_main.c
		${CODE}/renderer/tr_marks.c
		${CODE}/renderer/tr_mesh.c
		${CODE}/renderer/tr_model.c
		${CODE}/renderer/tr_noise.c
		${CODE}/renderer/tr_scene.c
		${CODE}/renderer/tr_shade.c
		${CODE}/renderer/tr_shade_calc.c
		${CODE}/renderer/tr_shader.c
		${CODE}/renderer/tr_shadows.c
		${CODE}/renderer/tr_sky.c
		${CODE}/renderer/tr_surface.c
		${CODE}/renderer/tr_world.c
	)

	set(JPEG_SOURCES
		${CODE}/jpeg-6/jcapimin.c
		${CODE}/jpeg-6/jchuff.c
		${CODE}/jpeg-6/jcinit.c
		${CODE}/jpeg-6/jccoefct.c
		${CODE}/jpeg-6/jccolor.c
		${CODE}/jpeg-6/jfdctflt.c
		${CODE}/jpeg-6/jcdctmgr.c
		${CODE}/jpeg-6/jcphuff.c
		${CODE}/jpeg-6/jcmainct.c
		${CODE}/jpeg-6/jcmarker.c
		${CODE}/jpeg-6/jcmaster.c
		${CODE}/jpeg-6/jcomapi.c
		${CODE}/jpeg-6/jcparam.c
		${CODE}/jpeg-6/jcprepct.c
		${CODE}/jpeg-6/jcsample.c
		${CODE}/jpeg-6/jdapimin.c
		${CODE}/jpeg-6/jdapistd.c
		${CODE}/jpeg-6/jdatasrc.c
		${CODE}/jpeg-6/jdcoefct.c
		${CODE}/jpeg-6/jdcolor.c
		${CODE}/jpeg-6/jddctmgr.c
		${CODE}/jpeg-6/jdhuff.c
		${CODE}/jpeg-6/jdinput.c
		${CODE}/jpeg-6/jdmainct.c
		${CODE}/jpeg-6/jdmarker.c
		${CODE}/jpeg-6/jdmaster.c
		${CODE}/jpeg-6/jdpostct.c
		${CODE}/jpeg-6/jdsample.c
		${CODE}/jpeg-6/jdtrans.c
		${CODE}/jpeg-6/jerror.c
		${CODE}/jpeg-6/jidctflt.c
		${CODE}/jpeg-6/jmemmgr.c
		${CODE}/jpeg-6/jmemnobs.c
		${CODE}/jpeg-6/jutils.c
	)

	set(SDL_SOURCES
		${CODE}/sdl/sdl_glimp.c
		${CODE}/sdl/sdl_input.c
		${CODE}/sdl/sdl_snd.c
	)

	add_executable(quake3
		${CLIENT_SOURCES}
		${SERVER_SOURCES}
		${QCOMMON_SOURCES}
		${BOTLIB_SOURCES}
		${RENDERER_SOURCES}
		${JPEG_SOURCES}
		${UNIX_SOURCES}
		${SDL_SOURCES}
	)
	target_compile_options(quake3 PRIVATE ${Q3_C_FLAGS})
	target_compile_definitions(quake3 PRIVATE ${Q3_DEFINES} USE_SDL)
	target_link_libraries(quake3 PRIVATE SDL2::SDL2 OpenGL::GL ${CMAKE_DL_LIBS} m)
	set_target_properties(quake3 PROPERTIES OUTPUT_NAME "quake3.${Q3_ARCH}")
	install(TARGETS quake3 RUNTIME DESTINATION .)
endif()

# --------------------------------------------------------------------------
# q3ded dedicated server
# --------------------------------------------------------------------------

if(BUILD_SERVER)
	add_executable(q3ded
		${SERVER_SOURCES}
		${QCOMMON_SOURCES}
		${BOTLIB_SOURCES}
		${UNIX_SOURCES}
		${CODE}/null/null_client.c
		${CODE}/null/null_input.c
		${CODE}/null/null_snddma.c
	)
	target_compile_options(q3ded PRIVATE ${Q3_C_FLAGS})
	target_compile_definitions(q3ded PRIVATE ${Q3_DEFINES} DEDICATED C_ONLY)
	target_link_libraries(q3ded PRIVATE ${CMAKE_DL_LIBS} m)
	set_target_properties(q3ded PROPERTIES OUTPUT_NAME "q3ded.${Q3_ARCH}")
	install(TARGETS q3ded RUNTIME DESTINATION .)
endif()

# --------------------------------------------------------------------------
# native baseq3 game modules
# --------------------------------------------------------------------------

if(BUILD_GAME_LIBRARIES)
	set(GAME_SOURCES
		${CODE}/game/ai_chat.c
		${CODE}/game/ai_cmd.c
		${CODE}/game/ai_dmnet.c
		${CODE}/game/ai_dmq3.c
		${CODE}/game/ai_main.c
		${CODE}/game/ai_team.c
		${CODE}/game/ai_vcmd.c
		${CODE}/game/bg_misc.c
		${CODE}/game/bg_pmove.c
		${CODE}/game/bg_slidemove.c
		${CODE}/game/g_active.c
		${CODE}/game/g_arenas.c
		${CODE}/game/g_bot.c
		${CODE}/game/g_client.c
		${CODE}/game/g_cmds.c
		${CODE}/game/g_combat.c
		${CODE}/game/g_items.c
		${CODE}/game/g_main.c
		${CODE}/game/g_mem.c
		${CODE}/game/g_misc.c
		${CODE}/game/g_missile.c
		${CODE}/game/g_mover.c
		${CODE}/game/g_session.c
		${CODE}/game/g_spawn.c
		${CODE}/game/g_svcmds.c
		${CODE}/game/g_syscalls.c
		${CODE}/game/g_target.c
		${CODE}/game/g_team.c
		${CODE}/game/g_trigger.c
		${CODE}/game/g_utils.c
		${CODE}/game/g_weapon.c
		${CODE}/game/q_math.c
		${CODE}/game/q_shared.c
	)

	set(CGAME_SOURCES
		${CODE}/game/bg_misc.c
		${CODE}/game/bg_pmove.c
		${CODE}/game/bg_slidemove.c
		${CODE}/cgame/cg_consolecmds.c
		${CODE}/cgame/cg_draw.c
		${CODE}/cgame/cg_drawtools.c
		${CODE}/cgame/cg_effects.c
		${CODE}/cgame/cg_ents.c
		${CODE}/cgame/cg_event.c
		${CODE}/cgame/cg_info.c
		${CODE}/cgame/cg_localents.c
		${CODE}/cgame/cg_main.c
		${CODE}/cgame/cg_marks.c
		${CODE}/cgame/cg_players.c
		${CODE}/cgame/cg_playerstate.c
		${CODE}/cgame/cg_predict.c
		${CODE}/cgame/cg_scoreboard.c
		${CODE}/cgame/cg_servercmds.c
		${CODE}/cgame/cg_snapshot.c
		${CODE}/cgame/cg_syscalls.c
		${CODE}/cgame/cg_view.c
		${CODE}/cgame/cg_weapons.c
		${CODE}/game/q_math.c
		${CODE}/game/q_shared.c
	)

	set(Q3UI_SOURCES
		${CODE}/game/bg_misc.c
		${CODE}/q3_ui/ui_addbots.c
		${CODE}/q3_ui/ui_atoms.c
		${CODE}/q3_ui/ui_cdkey.c
		${CODE}/q3_ui/ui_cinematics.c
		${CODE}/q3_ui/ui_confirm.c
		${CODE}/q3_ui/ui_connect.c
		${CODE}/q3_ui/ui_controls2.c
		${CODE}/q3_ui/ui_credits.c
		${CODE}/q3_ui/ui_demo2.c
		${CODE}/q3_ui/ui_display.c
		${CODE}/q3_ui/ui_gameinfo.c
		${CODE}/q3_ui/ui_ingame.c
		${CODE}/q3_ui/ui_loadconfig.c
		${CODE}/q3_ui/ui_main.c
		${CODE}/q3_ui/ui_menu.c
		${CODE}/q3_ui/ui_mfield.c
		${CODE}/q3_ui/ui_mods.c
		${CODE}/q3_ui/ui_network.c
		${CODE}/q3_ui/ui_options.c
		${CODE}/q3_ui/ui_playermodel.c
		${CODE}/q3_ui/ui_players.c
		${CODE}/q3_ui/ui_playersettings.c
		${CODE}/q3_ui/ui_preferences.c
		${CODE}/q3_ui/ui_qmenu.c
		${CODE}/q3_ui/ui_removebots.c
		${CODE}/q3_ui/ui_saveconfig.c
		${CODE}/q3_ui/ui_serverinfo.c
		${CODE}/q3_ui/ui_servers2.c
		${CODE}/q3_ui/ui_setup.c
		${CODE}/q3_ui/ui_sound.c
		${CODE}/q3_ui/ui_sparena.c
		${CODE}/q3_ui/ui_specifyserver.c
		${CODE}/q3_ui/ui_splevel.c
		${CODE}/q3_ui/ui_sppostgame.c
		${CODE}/q3_ui/ui_spskill.c
		${CODE}/q3_ui/ui_startserver.c
		${CODE}/q3_ui/ui_team.c
		${CODE}/q3_ui/ui_teamorders.c
		${CODE}/q3_ui/ui_video.c
		${CODE}/ui/ui_syscalls.c
		${CODE}/game/q_math.c
		${CODE}/game/q_shared.c
	)

	foreach(module qagame cgame ui)
		if(module STREQUAL "qagame")
			set(sources ${GAME_SOURCES})
			set(defines QAGAME)
		elseif(module STREQUAL "cgame")
			set(sources ${CGAME_SOURCES})
			set(defines CGAME)
		else()
			set(sources ${Q3UI_SOURCES})
			set(defines UI)
		endif()
		add_library(${module} MODULE ${sources})
		target_compile_options(${module} PRIVATE ${Q3_C_FLAGS})
		target_compile_definitions(${module} PRIVATE ${defines})
		target_link_libraries(${module} PRIVATE m)
		set_target_properties(${module} PROPERTIES
			PREFIX ""
			OUTPUT_NAME "${module}${Q3_ARCH}"
			LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/baseq3)
		install(TARGETS ${module} LIBRARY DESTINATION baseq3)
	endforeach()
endif()
