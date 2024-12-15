project('phtree', 'c')

cc = meson.get_compiler('c')

library_directory = meson.current_source_dir() + '/external/raylib'

phtree_dependencies = [
  cc.find_library('m'),
  cc.find_library('dl'),
  cc.find_library('pthread'),
  cc.find_library('raylib', dirs : library_directory),
]

include = [
  include_directories('.'),
  include_directories('external/cvector'),
  include_directories('external/raylib'),
  include_directories('external/pcg'),
  include_directories('external/phtree'),
]

source_files = [
  'source/main.c',
]

subdir ('external/pcg')
subdir ('external/phtree')

phtree_binary = executable (
  'phtree',
  source_files,
  include_directories : include,
  dependencies : [phtree_dependencies],
)
