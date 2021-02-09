project('quake2c', 'c', 'cpp',
        default_options : ['buildtype=debugoptimized',
                           'c_std=c17',
                           'cpp_std=c++2a', 
                           'warning_level=0'],
        license: 'GPL-3.0-or-later')

if get_option('KMQUAKE2')
  add_project_arguments('-DKMQUAKE2_ENGINE_MOD', language: ['c','cpp'])
endif

if get_option('X32')
  add_project_arguments('-m32', language: ['c', 'cpp'])
  add_project_link_arguments('-m32', language: ['c', 'cpp'])
endif

if get_option('ALLOW_DEBUGGING')
  add_project_arguments('-DALLOW_DEBUGGING=1', language: ['c','cpp'])
else
  add_project_arguments('-DALLOW_DEBUGGING=0', language: ['c','cpp'])
endif

if get_option('ALLOW_INSTRUMENTING')
  add_project_arguments('-DALLOW_INSTRUMENTING=1', language: ['c', 'cpp'])
endif

if get_option('ALLOW_PROFILING')
  add_project_arguments('-DALLOW_PROFILING=1', language: ['c', 'cpp'])
else
  add_project_arguments('-DALLOW_PROFILING=0', language: ['c', 'cpp'])
endif

if get_option('USE_GNU_OPCODE_JUMPING')
  add_project_arguments('-DUSE_GNU_OPCODE_JUMPING=1', language: ['c', 'cpp'])
else
  add_project_arguments('-DUSE_GNU_OPCODE_JUMPING=0', language: ['c', 'cpp'])
endif

inc_dirs = []
if host_machine.system() == 'windows'
  add_project_arguments('-DWINDOWS', language: ['c','cpp'])
  inc_dirs += 'windows'
else #extend above here for non-unix porting. This is unlikely to happen...
  add_project_arguments('-DUNIX', language: ['c','cpp'])
endif


sources = ['shared/shared.c',
           'g_file.cpp',
           'g_main.c',
           'g_save.c',
           'g_thread.cpp',
           'g_time.cpp',
           'vm.c',
           'vm_debug.c',
           'vm_ext.c',
           'vm_file.c',
           'vm_game.c',
           'vm_gi.c',
           'vm_hash.c',
           'vm_heap.c',
           'vm_list.c',
           'vm_math.c',
           'vm_mem.c',
           'vm_string.c',
           'vm_string_list.c',
           'vm_structlist.c']

shared_library('game', sources,
               name_prefix: '',
               include_directories: inc_dirs,
               dependencies: dependency('threads'))
