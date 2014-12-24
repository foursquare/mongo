import os

def configure(conf, env):
    root = os.path.dirname(__file__)

    # If you want to #define something
    env.Append(CPPDEFINES=dict(MONGO_HELLO_VERSION=1))

    # If you want to set an environment variable for the compiler
    env['MONGO_BUILD_HELLO'] = True

    distsrc = env.Dir(root).Dir('distsrc')
    docs = env.Dir(root).Dir('docs')
    env.Append(MODULE_BANNERS=[
            distsrc.File('LICENSE.txt'),
            ])
    env.Append(ARCHIVE_ADDITIONS=[
            docs.File('README-hello.txt')
            ])
    env.Append(ARCHIVE_ADDITION_DIR_MAP={
            str(env.Dir(root).Dir('docs')): "hello"
            })

    def addFileInExtraPath(file_name):
        paths = env['EXTRABINPATH']
        for path in paths:
            full_file_name = os.path.join(os.path.normpath(path.lower()), file_name)
            if os.path.exists(full_file_name):
                env.Append(ARCHIVE_ADDITIONS=[full_file_name])

                env.Append(ARCHIVE_ADDITION_DIR_MAP={
                        os.path.normpath(path.lower()): "bin"
                        })
                break
