project('dm', 'c')
cc = meson.get_compiler('c')
drm = dependency('libdrm')
libinput = dependency('libinput')
udev = dependency('libudev')
libev = cc.find_library('ev', required: true)
m = cc.find_library('m', required: true)
fc = dependency('fontconfig', required: true)
ft = dependency('freetype2', required: true)

dm_src = ['main.c', 'scene.c', 'input.c', 'var.c', 'render.c',
          'backend_drm.c', 'user.c', 'image.c', 'inih/ini.c', 'font.c']
executable('dm', dm_src,
           dependencies: [drm, libinput, udev, libev, m, fc, ft],
           include_directories: [include_directories('inih'), include_directories('stb')],
           c_args: [ '-D_GNU_SOURCE' ])
