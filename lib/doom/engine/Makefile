# SPDX-License-Identifier: GPL-2.0+
#
# Doom engine (doomgeneric) object list. Third-party GPL source, so
# warnings are silenced and a private libc shim is placed ahead of the
# include path.

ccflags-y := -w -fcommon -I$(srctree)/lib/doom/compat
ccflags-y += -DDOOMGENERIC_RESX=960 -DDOOMGENERIC_RESY=600

# m_fixed.c calls abs() without including a header for it. Force dg_stdlib.h
# into every file so that it always has both the macro and a prototype:
# defining abs on the command line alone leaves the call undeclared, which
# newer compilers reject outright, and sandbox would otherwise quietly take
# abs() from the host C library while a real board has none to take it from.
ccflags-y += -Dabs=dg_abs -Dlabs=dg_labs -include dg_abs.h
ccflags-y += -DNORMALUNIX -DLINUX

obj-y += am_map.o
obj-y += d_event.o
obj-y += d_items.o
obj-y += d_iwad.o
obj-y += d_loop.o
obj-y += d_main.o
obj-y += d_mode.o
obj-y += d_net.o
obj-y += doomdef.o
obj-y += doomgeneric.o
obj-y += doomstat.o
obj-y += dstrings.o
obj-y += dummy.o
obj-y += f_finale.o
obj-y += f_wipe.o
obj-y += g_game.o
obj-y += hu_lib.o
obj-y += hu_stuff.o
obj-y += i_cdmus.o
obj-y += i_endoom.o
obj-y += i_input.o
obj-y += i_joystick.o
obj-y += i_scale.o
obj-y += i_sound.o
obj-y += i_system.o
obj-y += i_timer.o
obj-y += i_video.o
obj-y += info.o
obj-y += m_argv.o
obj-y += m_bbox.o
obj-y += m_cheat.o
obj-y += m_config.o
obj-y += m_controls.o
obj-y += m_fixed.o
obj-y += m_menu.o
obj-y += m_misc.o
obj-y += m_random.o
obj-y += memio.o
obj-y += p_ceilng.o
obj-y += p_doors.o
obj-y += p_enemy.o
obj-y += p_floor.o
obj-y += p_inter.o
obj-y += p_lights.o
obj-y += p_map.o
obj-y += p_maputl.o
obj-y += p_mobj.o
obj-y += p_plats.o
obj-y += p_pspr.o
obj-y += p_saveg.o
obj-y += p_setup.o
obj-y += p_sight.o
obj-y += p_spec.o
obj-y += p_switch.o
obj-y += p_telept.o
obj-y += p_tick.o
obj-y += p_user.o
obj-y += r_bsp.o
obj-y += r_data.o
obj-y += r_draw.o
obj-y += r_main.o
obj-y += r_plane.o
obj-y += r_segs.o
obj-y += r_sky.o
obj-y += r_things.o
obj-y += s_sound.o
obj-y += sha1.o
obj-y += sounds.o
obj-y += st_lib.o
obj-y += st_stuff.o
obj-y += statdump.o
obj-y += tables.o
obj-y += v_video.o
obj-y += w_checksum.o
obj-y += w_file.o
obj-y += w_file_stdc.o
obj-y += w_main.o
obj-y += w_wad.o
obj-y += wi_stuff.o
obj-y += z_zone.o
