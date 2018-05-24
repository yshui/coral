project('dm', 'c')
cc = meson.get_compiler('c')
drm = dependency('libdrm')
libinput = dependency('libinput')
udev = dependency('libudev')
libev = cc.find_library('ev', required: true)
m = cc.find_library('m', required: true)
executable('dm', ['main.c', 'scene.c', 'input.c', 'interpolate.c', 'render.c', 'backend_drm.c', 'user.c'],
           dependencies: [drm, libinput, udev, libev, m], include_directories: include_directories('stb'), c_args: [ '-D_GNU_SOURCE' ])
