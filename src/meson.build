srcs = [
    'main.cpp',
    'editor.cpp',
    'erow.cpp',
    'append_buffer.cpp',
    'syntax_highlight.cpp',
    'find_mode.cpp',
]
if host_machine.system() == 'windows'
    srcs += 'term_util_win32.cpp'
else
    srcs += 'term_util_linux.cpp'
endif

executable(
    'kilo',
    srcs,
    install: true,
)
