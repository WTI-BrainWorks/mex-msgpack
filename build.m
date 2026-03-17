% build the static msgpack first from the submodule
system('cmake -S msgpack-c -B build -DCMAKE_BUILD_TYPE=Release -DMSGPACK_ENABLE_STATIC=ON -DMSGPACK_ENABLE_SHARED=OFF');
system('cmake --build build --config Release');

if ispc
    libfile = './build/Release/msgpack-c.lib';
else
    libfile = './build/libmsgpack-c.a';
% ismac (??)
end

mex('-I"./msgpack-c/include"', '-I"./build/include"', '-I"./build/include/msgpack"', ...
    libfile, 'msgpack_mex.c', '-output', 'msgpack', '-outdir', './dist', '-R2018a');
