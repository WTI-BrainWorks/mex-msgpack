% build the static msgpack first from the submodule
system(['cmake -S msgpack-c -B build -DCMAKE_BUILD_TYPE=Release ' ...
        '-DCMAKE_INSTALL_PREFIX=./inst ' ...
        '-DMSGPACK_ENABLE_STATIC=ON -DMSGPACK_ENABLE_SHARED=OFF -DMSGPACK_BUILD_EXAMPLES=OFF']);
system('cmake --build build --config Release');
system('cmake --install build');

if ispc
    libfile = './build/Release/msgpack-c.lib';
else
    libfile = './inst/lib/libmsgpack-c.a';
% ismac (??)
end

mex('-I"./inst/include"', libfile, 'msgpack_mex.c', '-output', './dist/msgpack', '-R2018a');
