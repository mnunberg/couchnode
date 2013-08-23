{
  'targets': [{
    'target_name': 'couchbase_impl',
    'conditions': [
      [ 'OS=="win"', {
        'variables': {
          'couchbase_root%': 'C:/couchbase'
        },
        'include_dirs': [
          '<(couchbase_root)/include/',
        ],
        'link_settings': {
          'libraries': [
            '-l<(couchbase_root)/lib/libcouchbase.lib',
          ],
        },
        'copies': [{
          'files': [ '<(couchbase_root)/bin/libcouchbase.dll' ],
          'destination': '<(module_root_dir)/build/Release/',
        },],
        'configurations': {
          'Release': {
            'msvs_settings': {
              'VCCLCompilerTool': {
                'ExceptionHandling': '2',
                'RuntimeLibrary': 0,
              },
            },
          },
        },
      }],
      ['OS=="mac"', {
        'xcode_settings': {
          'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
        },
      }],
      ['OS!="win"', {
        'variables' : {
            'couchbase_root%' : '""'
        },

        'link_settings': {
          'libraries': [
            '$(EXTRA_LDFLAGS)',
            '-lcouchbase',
          ],
        },
        'cflags': [
          '-g',
          '-fPIC',
          '-Wall',
          '-Wextra',
          '-Wno-unused-variable',
          '-Wno-unused-function',
          '$(EXTRA_CFLAGS)',
          '$(EXTRA_CPPFLAGS)',
          '$(EXTRA_CXXFLAGS)',
        ],
        'cflags_c':[
          '-pedantic',
          '-std=gnu99',
        ],
        'cflags!': [
          '-fno-exceptions',
        ],
        'cflags_cc!': [
          '-fno-exceptions',
        ],
        'conditions': [
            [ 'couchbase_root!=""', {
                'include_dirs': [ '<(couchbase_root)/include' ],
                'libraries+': [
                    '-L<(couchbase_root)/lib',
                    '-Wl,-rpath=<(couchbase_root)/lib'
                ]
            }]
        ],
      }]
    ],
    'defines': ['LCBUV_EMBEDDED_SOURCE'],
    'sources': [
      'src/couchbase_impl.cc',
      'src/control.cc',
      'src/namemap.cc',
      'src/cookie.cc',
      'src/commandbase.cc',
      'src/commands.cc',
      'src/exception.cc',
      'src/options.cc',
      'src/cas.cc',
      'src/uv-plugin-all.c'
    ],
    'include_dirs': [
      './',
      './src/io'
    ],
  }]
}
