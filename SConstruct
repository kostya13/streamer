VariantDir('build', 'src')
env = Environment()
env.Append(CCFLAGS = '-std=c++11')
env.Append(LINKFLAGS = '-pthread')
env.Program('build/server.cpp')
env.Program('build/client.cpp')

