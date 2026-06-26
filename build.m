function build()
% BUILD  Compile the msgpack MEX for MATLAB or Octave.
%   Builds the bundled msgpack-c static library with CMake, then compiles
%   msgpack_mex.c against it. Works on Windows, Linux and macOS, under both
%   MATLAB and Octave. Errors (non-zero exit under -batch / --eval) on failure.

    here = fileparts(mfilename('fullpath'));
    old = cd(here);
    restore = onCleanup(@() cd(old));

    is_octave = exist('OCTAVE_VERSION', 'builtin') ~= 0;

    % --- build the static msgpack-c library from the submodule ---
    % Pin CMAKE_INSTALL_LIBDIR=lib so the archive always lands in inst/lib --
    % msgpack-c uses GNUInstallDirs, which would otherwise install to inst/lib64
    % on multilib distros (Fedora/RHEL/SUSE) and break the libfile lookup below.
    cfg = ['cmake -S msgpack-c -B build -DCMAKE_BUILD_TYPE=Release ' ...
           '-DCMAKE_INSTALL_PREFIX=./inst -DCMAKE_INSTALL_LIBDIR=lib ' ...
           '-DMSGPACK_ENABLE_STATIC=ON -DMSGPACK_ENABLE_SHARED=OFF ' ...
           '-DMSGPACK_BUILD_EXAMPLES=OFF'];
    gen = getenv('MEX_CMAKE_GENERATOR');
    if ~isempty(gen)
        cfg = [cfg ' -G "' gen '"'];
    elseif ispc && is_octave
        % Octave on Windows ships a MinGW gcc toolchain; build the library with
        % it so the static archive is link-compatible with mkoctfile. Under MSYS2
        % set MEX_CMAKE_GENERATOR=Ninja instead -- cmake's "MinGW Makefiles"
        % generator refuses to run while sh.exe is on PATH.
        cfg = [cfg ' -G "MinGW Makefiles"'];
    end
    % A build/ left over from a different generator makes cmake refuse to
    % reconfigure ("does not match the generator used previously"); on a
    % configure failure, wipe build/ and try once more.
    if system(cfg) ~= 0
        fprintf('cmake configure failed; wiping build/ and retrying...\n');
        if exist('build', 'dir'); rmdir('build', 's'); end
        run_cmd(cfg);
    end
    run_cmd('cmake --build build --config Release');
    run_cmd('cmake --install build');

    % --- locate the installed static library ---
    if ispc && ~is_octave
        libfile = fullfile('inst', 'lib', 'msgpack-c.lib');    % MSVC
    else
        libfile = fullfile('inst', 'lib', 'libmsgpack-c.a');   % MinGW / Unix
    end
    assert(exist(libfile, 'file') ~= 0, 'build:lib', ...
           'static library not found: %s', libfile);

    % --- compile the MEX ---
    % No -R2018a: the code uses only the classic mx* API (mxGetData etc.), so it
    % builds against the default API everywhere from old Octave (4.4) to current
    % MATLAB. -R2018a (interleaved complex) would exclude old Octave and isn't
    % needed since complex arrays are rejected up front.
    if ~exist('dist', 'dir'); mkdir('dist'); end
    mex('-I./inst/include', libfile, 'msgpack_mex.c', ...
        '-output', fullfile('dist', 'msgpack'));

    fprintf('build complete: %s\n', fullfile('dist', 'msgpack'));
end

function run_cmd(cmd)
    status = system(cmd);
    assert(status == 0, 'build:cmd', 'command failed (status %d): %s', status, cmd);
end
