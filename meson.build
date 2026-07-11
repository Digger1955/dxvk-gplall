project('dxvk', ['c', 'cpp'], version : '2.6.8', meson_version : '>= 0.58', default_options : [ 'cpp_std=c++17', 'b_vscrt=static_from_buildtype', 'warning_level=2' ])

pkg = import('pkgconfig')
cpu_family = target_machine.cpu_family()
platform   = target_machine.system()
fs = import('fs')

cpp = meson.get_compiler('cpp')
cc = meson.get_compiler('c')
dxvk_is_msvc = cpp.get_argument_syntax() == 'msvc'

compiler_args = [
  '-O3', # Optimizes for speed. -Os fails to compile due to compiler bug
  '-pipe', # Enables use of pipes instead of temporary files for communication between the various stages of compilation; faster compilation, uses more memory, no impact on code
  '-march=x86-64', # SSE2 required
  '-mtune=x86-64', # Tuning for SSE2 CPUs 
  '-msse',
  '-msse2',
  '-mfpmath=sse',
  '-fno-semantic-interposition', # Semantic interposition prevents a range of inter-procedural optimizations
  '-ffunction-sections', # Place each function into its own section, improves locality of reference in the instruction space
  '-fdata-sections', # Place each data item into its own section, improves locality of reference in the instruction space
  '-flto=auto', # Enables FullLTO for Clang and GCC
  '-Wimplicit-fallthrough',
  '-Wno-deprecated', # Silences warnings about use of -mtune=x86-64
  # gcc
  '-fuse-linker-plugin', # Enabled by default in newer GCC toolchains
  '-Wno-missing-field-initializers',
  '-Wno-unused-parameter',
  '-Wno-misleading-indentation',
  '-Wno-cast-function-type', # Needed for GetProcAddress.
  # clang; fails to compile due to non-compliance with Clang requirements.
  '-fwhole-program-vtables', # Requires FullLTO, enables single-implementation devirtualization and virtual constant propagation
  '-Wno-unused-private-field',
  '-Wno-microsoft-exception-spec',
  '-Wno-extern-c-compat',
  '-Wno-unused-const-variable',
  '-Wno-missing-braces',
]

link_args = [
  '-s', # Removes all symbol table and relocation information from the executable
  '-Wl,--gc-sections', # Removes function and data duplicates from sections
]

if get_option('build_id')
  link_args += [
    '-Wl,--build-id',
  ]
endif

dxvk_include_dirs = ['./include']
if fs.is_dir('./include/vulkan/include')
  dxvk_include_dirs += ['./include/vulkan/include']
elif not cpp.check_header('vulkan/vulkan.h')
  error('Missing Vulkan-Headers')
endif
if fs.is_dir('./include/spirv/include')
  dxvk_include_dirs += ['./include/spirv/include']
elif not cpp.check_header('spirv/unified1/spirv.hpp')
  error('Missing SPIRV-Headers')
endif

dep_displayinfo = dependency(
  'libdisplay-info',
  version: ['>= 0.0.0', '< 0.2.0'],
  fallback: ['libdisplay-info', 'di_dep'],
  default_options: ['default_library=static'],
)

if platform == 'windows'
  dxvk_so_version = {'name_prefix': ''}

  compiler_args += [
    '-DNOMINMAX',
    '-D_WIN32_WINNT=0xa00',
  ]

  if not dxvk_is_msvc
    link_args += [
      '-static',
      '-static-libgcc',
      '-static-libstdc++',
      '-lsynchronization',
      # We need to set the section alignment for debug symbols to
      # work properly as well as avoiding a memcpy from the Wine loader.
      '-Wl,--file-alignment=4096',
    ]

    # Wine's built-in back traces only work with dwarf4 symbols
    if get_option('debug')
      compiler_args += [
        '-gdwarf-4',
      ]
    endif

    if cpu_family == 'x86'
      # Enable stdcall fixup on 32-bit
      link_args += [
        '-Wl,--enable-stdcall-fixup',
        '-Wl,--kill-at',
      ]
      # Fix stack alignment issues with mingw on 32-bit
      compiler_args += [
        '-mpreferred-stack-boundary=2'
      ]
    endif
  else
    # Windows (MSVC) Compiler Options
    compiler_args += [
      '/arch:SSE2', # MSVC default, SSE2 required
      '/favor:blend', # MSVC default, designed to give the best performance across a broad range of x86 and x64 processors
      '/O1', # Minimizes Size, eqvivalent to - /Og /Os /Oy /Ob2 /GF /Gy. For out-of-order CPUs fitting a code segment into the instruction cache is preffered to fastest code
      '/Oi', # Replaces function calls with intrinsic or otherwise special forms of the function
      '/Ob3', # More aggressive inlining than /Ob2, but has the same restrictions
      '/GL', # Enables whole program optimization, required for LTCG
      '/Gw', # Package global data in COMDAT sections for optimization, required for LTCG
      '/Qpar',# Enables automatic parallelization of loops
      '/Qpar-report:1', # Outputs an informational message for loops that are parallelized
      '/Qvec-report:1' # Outputs an informational message for loops that are vectorized
    ]
    # MSVC Linker Options
    link_args += [
      '/FILEALIGN:4096',
      '/LTCG', # Performs a Link-Time Code Generation
      '/INCREMENTAL:NO', # Performs a full link LTCG
      '/OPT:REF,ICF', # REF - eliminates functions and data that are never referenced; ICF - performs identical COMDAT folding
      '/DEBUG:NONE', # Doesn't generate a PDB
      '-lsynchronization'
    ]
  endif

  lib_d3d9    = cpp.find_library('d3d9')
  lib_d3d11   = cpp.find_library('d3d11')
  lib_dxgi    = cpp.find_library('dxgi')
  lib_gdi32   = cpp.find_library('gdi32')
  lib_version = cpp.find_library('version')

  if dxvk_is_msvc
    res_ext = '.res'
    wrc = find_program('rc')
    wrc_generator = generator(wrc,
      output    : [ '@BASENAME@' + res_ext ],
      arguments : [ '/fo', '@OUTPUT@', '@INPUT@' ],
    )
  else
    res_ext = '.o'
    wrc = find_program('windres')
    wrc_generator = generator(wrc,
      output    : [ '@BASENAME@' + res_ext ],
      arguments : [ '-i', '@INPUT@', '-o', '@OUTPUT@' ],
    )
  endif

  dxvk_name_prefix = ''
  compiler_args += ['-DDXVK_WSI_WIN32']
else
  dxvk_abi_version = '0'
  dxvk_version_raw = meson.project_version().strip('v').split('.')
  dxvk_version = [ dxvk_version_raw[0] ]
  foreach i : [ 1, 2 ]
    padded = dxvk_version_raw[i]
    if padded.to_int() < 10
      padded = '0' + padded
    endif
    dxvk_version += [ padded ]
  endforeach
  dxvk_so_version = {'version': dxvk_abi_version + '.' + dxvk_version[0] + dxvk_version[1] + dxvk_version[2]}

  wrc           = find_program('touch')
  wrc_generator = generator(wrc, output : [ '@BASENAME@_ignored.h' ], arguments : [ '@OUTPUT@' ] )

  dxvk_include_dirs += [
    './include/native',
    './include/native/windows',
    './include/native/directx'
  ]

  lib_sdl3 = dependency('sdl3', required: get_option('native_sdl3'))
  lib_sdl2 = dependency('sdl2', required: get_option('native_sdl2'))
  lib_glfw = dependency('glfw3', required: get_option('native_glfw'))
  if lib_sdl3.found()
    compiler_args += ['-DDXVK_WSI_SDL3']
  endif
  if lib_sdl2.found()
    compiler_args += ['-DDXVK_WSI_SDL2']
  endif
  if lib_glfw.found()
    compiler_args += ['-DDXVK_WSI_GLFW']
  endif
  if (not lib_sdl3.found() and not lib_sdl2.found() and not lib_glfw.found())
    error('SDL3, SDL2, or GLFW are required to build dxvk-native')
  endif
  
  dxvk_name_prefix = 'dxvk_'
  dxvk_pkg_prefix = 'dxvk-'

  link_args += [
    '-static-libgcc',
    '-static-libstdc++',
  ]
endif

dxvk_include_path = include_directories(dxvk_include_dirs)

add_project_arguments(cpp.get_supported_arguments(compiler_args), language: 'cpp')
add_project_arguments(cc.get_supported_arguments(compiler_args), language: 'c')
add_project_link_arguments(cpp.get_supported_link_arguments(link_args), language: 'cpp')
add_project_link_arguments(cc.get_supported_link_arguments(link_args), language: 'c')

exe_ext = ''
def_spec_ext = '.def'

glsl_compiler = find_program('glslang', 'glslangValidator')
glsl_args = [
  '--quiet',
  '--target-env', 'vulkan1.3',
  '--vn', '@BASENAME@',
  '--depfile', '@DEPFILE@',
  '@INPUT@',
  '-o', '@OUTPUT@',
]
glsl_generator = generator(
  glsl_compiler,
  output    : [ '@BASENAME@.h' ],
  depfile   : '@BASENAME@.h.d',
  arguments : glsl_args,
)

dxvk_version = vcs_tag(
  command: ['git', 'describe', '--dirty=-gplall'],
  input:  'version.h.in',
  output: 'version.h',
)

conf_data = configuration_data()
conf_data.set('BUILD_COMPILER', cpp.get_id())
conf_data.set('BUILD_COMPILER_VERSION', cpp.version())
conf_data.set('BUILD_TARGET', cpu_family)
dxvk_buildenv = configure_file(
  configuration : conf_data,
  input:  'buildenv.h.in',
  output: 'buildenv.h',
)

if platform != 'windows'
  subdir('include/native')
endif

subdir('src')
