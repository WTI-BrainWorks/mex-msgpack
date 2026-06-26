function run_tests()
% RUN_TESTS  Comprehensive test suite for the msgpack MEX wrapper.
%
%   Run from anywhere:  run_tests
%
% Tests are written against the wrapper's actual contract, not naive
% round-trip identity. Where pack/unpack is lossless the test asserts
% identity; where the wrapper intentionally transforms data (array shape
% is dropped, integer arrays widen to double, etc.) the test asserts the
% transformed result so the documented behavior stays pinned down.

    global TST_PASS TST_FAIL
    TST_PASS = 0;
    TST_FAIL = 0;

    here = fileparts(mfilename('fullpath'));
    addpath(fullfile(here, 'dist'));

    % make sure the built MEX answers -- if it isn't built, this call hits the
    % inert help-shim dist/msgpack.m (or nothing) and errors with a clear message
    try
        msgpack('pack', 0);
    catch
        error('run_tests:noMEX', 'msgpack MEX not built or not callable; run build first.');
    end

    rt = @(x) msgpack('unpack', msgpack('pack', x));

    %% ===================== scalars: lossless round-trip =====================
    eqn('double scalar',          rt(3.14),  3.14);
    eqn('double zero',            rt(0),     0);
    eqn('double negative',        rt(-2.5),  -2.5);
    isa_('double scalar class',   rt(3.14),  'double');
    eqn('logical true',           rt(true),  true);
    eqn('logical false',          rt(false), false);
    isa_('logical class',         rt(true),  'logical');

    %% ===================== special float values =====================
    eqn('NaN',           rt(NaN),  NaN);
    eqn('Inf',           rt(Inf),  Inf);
    eqn('-Inf',          rt(-Inf), -Inf);
    eqn('vector w/ NaN/Inf', rt([1 NaN Inf -Inf]), [1 NaN Inf -Inf]);

    %% ===================== strings =====================
    eqn('char string',   rt('hello world'), 'hello world');
    eqn('single char',   rt('x'), 'x');
    try
        ec = rt('');
        ok('empty char', ischar(ec) && isempty(ec));
    catch ec_err
        ok('empty char', false);
        fprintf(2, '        empty char threw: %s\n', ec_err.identifier);
    end
    eqn('punctuation',   rt('a-b_c.d!'), 'a-b_c.d!');

    %% ===================== numeric vectors & shape behavior =====================
    eqn('double row vector',  rt([1 2 3]), [1 2 3]);
    % column vectors and matrices come back as row vectors (shape is dropped)
    eqn('column -> row',      rt([1;2;3]), [1 2 3]);
    eqn('matrix -> row (col-major)', rt([1 2; 3 4]), [1 3 2 4]);
    eqn('large vector',       rt(1:1000), 1:1000);

    %% ===================== empty values =====================
    % empty numeric/logical arrays pack as nil and decode to [] (an empty double)
    eqn('empty [] -> []',          rt([]),            []);
    isa_('empty [] class',         rt([]),            'double');
    eqn('empty single -> []',      rt(single([])),    []);
    eqn('empty int32 -> []',       rt(int32([])),     []);
    eqn('empty logical -> []',     rt(logical([])),   []);   % previously emitted no bytes
    % these empties keep their own (non-nil) encoding and class (see other sections
    % for empty char / empty uint8); a cell stays an empty array
    ec_cell = rt({});
    ok('empty cell {} -> {}',      iscell(ec_cell) && isempty(ec_cell));

    %% ===================== integer types =====================
    % scalar integers come back as uint64 (>=0) or int64 (<0)
    isa_('int32 scalar -> uint64',  rt(int32(5)),  'uint64');
    eqn('int32 scalar value',       rt(int32(5)),  uint64(5));
    isa_('int32 neg -> int64',      rt(int32(-5)), 'int64');
    eqn('int32 neg value',          rt(int32(-5)), int64(-5));
    eqn('int16 -> uint64',          rt(int16(100)), uint64(100));
    eqn('uint16 -> uint64',         rt(uint16(100)), uint64(100));
    eqn('int8 neg -> int64',        rt(int8(-3)), int64(-3));
    % edge values
    eqn('uint64 max',  rt(intmax('uint64')), intmax('uint64'));
    eqn('int64 min',   rt(intmin('int64')),  intmin('int64'));
    eqn('int64 max',   rt(intmax('int64')),  intmax('int64'));
    % integer ARRAYS widen to double
    isa_('int32 array -> double',   rt(int32([1 2 3])), 'double');
    eqn('int32 array value',        rt(int32([1 2 3])), [1 2 3]);
    eqn('int32 neg array value',    rt(int32([-1 -2 -3])), [-1 -2 -3]);

    %% ===================== uint8 <-> binary (lossless) =====================
    isa_('uint8 array -> uint8',    rt(uint8([1 2 3])), 'uint8');
    eqn('uint8 array value',        rt(uint8([1 2 3])), uint8([1 2 3]));
    eqn('uint8 scalar',             rt(uint8(7)), uint8(7));
    eqn('uint8 empty -> uint8',     class(rt(uint8([]))), 'uint8');

    %% ===================== single (packs as float32, returns double) =====================
    isa_('single scalar -> double', rt(single(3.5)), 'double');
    eqn('single scalar value',      rt(single(3.5)), 3.5);
    eqn('single vector value',      rt(single([1.5 2.5 3.5])), [1.5 2.5 3.5]);

    %% ===================== logical arrays =====================
    isa_('logical array -> logical', rt([true false true]), 'logical');
    eqn('logical array value',       rt([true false true]), [true false true]);
    eqn('logical column -> row',     rt([true; false]), [true false]);

    %% ===================== cells =====================
    % homogeneous-numeric cell collapses to a numeric row vector
    eqn('cell of numbers -> vector', rt({1, 2, 3}), [1 2 3]);
    eqn('mixed double+int cell -> vector', rt({1, int32(2)}), [1 2]);
    % cells with strings stay cells
    eqn('cell of strings',           rt({'a', 'bb', 'ccc'}), {'a', 'bb', 'ccc'});
    eqn('heterogeneous cell',        rt({1, 'two', true}), {1, 'two', true});
    eqn('bool-led heterogeneous',    rt({true, 1}), {true, 1});
    eqn('nested cell',               rt({{1,2},{'a'}}), {[1 2], {'a'}});

    %% ===================== structs =====================
    s1 = struct('a', 1, 'b', 'c');
    eqn('struct scalar',             rt(s1), s1);
    s2 = struct('a', [1 5 6], 'b', 'foo');
    eqn('struct w/ vector field',    rt(s2), s2);
    s3 = struct('a', struct('a', 3, 'b', 500));
    eqn('nested struct',             rt(s3), s3);
    s4 = struct('s', 'txt', 'n', 42, 'flag', true, 'v', [1 2 3]);
    eqn('struct mixed fields',       rt(s4), s4);

    % struct arrays
    sa = struct('a', {1, 2, 3});
    eqn('struct array (numeric)',    rt(sa), sa);
    sb = struct('thbar', {'300', '4'}, 'ah', {[5 5], [5 5]});
    eqn('struct array (mixed)',      rt(sb), sb);
    % empty struct array packs as an empty array (0 maps), matching how an
    % N-element struct array packs as N maps; decodes to an empty cell
    eqn('empty struct array -> 0x90', msgpack('pack', struct('a', {})), uint8(144));
    esa = rt(struct('a', {}));
    ok('empty struct array -> {}',   iscell(esa) && isempty(esa));
    % heterogeneous array-of-maps decodes to a cell of structs (no field dropped),
    % not a struct array built from only the first map's keys
    het = msgpack('unpack', uint8([146 129 161 97 1 130 161 97 2 161 98 3])); % [{a:1},{a:2,b:3}]
    ok('heterogeneous maps -> cell', iscell(het) && numel(het) == 2 ...
        && isstruct(het{2}) && isfield(het{2}, 'b') && het{2}.b == 3);
    % an array-of-maps with an over-long (>63 char) key falls back to a cell
    % instead of handing an invalid name to mxCreateStructMatrix
    longkey = msgpack('unpack', uint8([145 129 217 64 repmat(uint8('a'), 1, 64) 1])); % [{<64 'a'>:1}]
    ok('over-long map key -> cell (no crash)', iscell(longkey));
    % an empty-string key is not a valid field name; fall back to a cell
    emptykey = msgpack('unpack', uint8([145 129 160 1])); % [{"":1}]
    ok('empty-string map key -> cell (no crash)', iscell(emptykey));

    % the original smoke examples, now actually checked
    eqn('orig example: cell+structarray', ...
        rt({'foo', struct('thbar', {'300', '4'}, 'ah', [5 5])}), ...
        {'foo', struct('thbar', {'300', '4'}, 'ah', [5 5])});

    %% ===================== decode raw bytes (external interop) =====================
    eqn('decode nil',        msgpack('unpack', uint8(192)),        []);
    eqn('decode false',      msgpack('unpack', uint8(194)),        false);
    eqn('decode true',       msgpack('unpack', uint8(195)),        true);
    eqn('decode +fixint',    msgpack('unpack', uint8(5)),          uint64(5));
    eqn('decode -fixint',    msgpack('unpack', uint8(255)),        int64(-1));
    eqn('decode float64',    msgpack('unpack', uint8([203 63 248 0 0 0 0 0 0])), 1.5);
    eqn('decode fixstr',     msgpack('unpack', uint8([162 104 105])), 'hi');
    eqn('decode int array',  msgpack('unpack', uint8([147 1 2 3])),   [1 2 3]);
    eqn('decode bin',        msgpack('unpack', uint8([196 3 1 2 3])), uint8([1 2 3]));

    m = msgpack('unpack', uint8([129 161 97 1]));   % {"a": 1}
    ok('decode map -> struct.a', isstruct(m) && isfield(m, 'a') && m.a == uint64(1));

    ext = msgpack('unpack', uint8([212 7 42]));     % fixext1, type 7, data 0x2a
    ok('decode ext struct', isstruct(ext) && isfield(ext,'type') && isfield(ext,'data') ...
        && ext.type == 7 && isa(ext.type,'int8') ...
        && ext.data == 42 && isa(ext.data,'uint8'));

    %% ===================== pack encoding (byte-level, against the spec) =====================
    eqn('pack true',     msgpack('pack', true),           uint8(195));
    eqn('pack false',    msgpack('pack', false),          uint8(194));
    eqn('pack +fixint',  msgpack('pack', int8(5)),        uint8(5));
    eqn('pack double 1', msgpack('pack', 1.0),            uint8([203 63 240 0 0 0 0 0 0]));
    eqn('pack fixstr',   msgpack('pack', 'hi'),           uint8([162 104 105]));
    eqn('pack bin',      msgpack('pack', uint8([1 2 3])), uint8([196 3 1 2 3]));
    eqn('pack map',      msgpack('pack', struct('a', true)), uint8([129 161 97 195]));

    %% ===================== error handling (clean errors, no crash) =====================
    throws('unpack non-uint8',      'msgpack:InvalidInput',         @() msgpack('unpack', 5));
    throws('unknown command',       'msgpack:InvalidCommand',       @() msgpack('frob', 1));
    throws('too few args',          'msgpack:Usage',                @() msgpack('pack'));
    throws('pack complex',          'msgpack:pack:UnsupportedType', @() msgpack('pack', 1 + 2i));
    throws('pack unsupported type', 'msgpack:pack:UnsupportedType', @() msgpack('pack', @sin));
    throws('unpack empty bytes',    'msgpack:UnpackFailed',         @() msgpack('unpack', uint8([])));
    throws('unpack truncated',      'msgpack:UnpackFailed',         @() msgpack('unpack', uint8([147 1])));
    throws('unpack reserved byte',  'msgpack:UnpackFailed',         @() msgpack('unpack', uint8(193)));   % 0xc1, never used
    % trailing bytes after the first object are accepted; the first value is returned
    eqn('trailing bytes -> first value', msgpack('unpack', uint8([1 2])), uint64(1));
    throws('unpack non-string key', 'msgpack:unpack:InvalidKey',    @() msgpack('unpack', uint8([129 1 2])));

    %% ===================== performance smoke (timing only) =====================
    payloads = { 1, [1 2 5], struct('a', 1, 'b', 'c'), ...
                 struct('a', [1 5 6], 'b', 'foo'), ...
                 {'foo', struct('thbar', {'300', '4'}, 'ah', [5 5])} };
    N = 2000;
    t = tic;
    for i = 1:N
        for k = 1:numel(payloads)
            msgpack('unpack', msgpack('pack', payloads{k}));
        end
    end
    el = toc(t);
    fprintf('perf: %d round-trips in %.3fs (%.1f us each)\n', ...
            N*numel(payloads), el, 1e6*el/(N*numel(payloads)));

    %% ===================== summary =====================
    fprintf('\n==== %d passed, %d failed ====\n', TST_PASS, TST_FAIL);
    if TST_FAIL > 0
        error('run_tests:failed', '%d test(s) failed', TST_FAIL);
    end
end

% ---- assertion harness ----
% Subfunctions + globals rather than nested functions: Octave (esp. older
% releases like 4.4) does not reliably support nested functions that share the
% parent workspace, whereas globals work everywhere.
function ok(name, cond)
    global TST_PASS TST_FAIL
    if cond
        TST_PASS = TST_PASS + 1;
    else
        TST_FAIL = TST_FAIL + 1;
        fprintf(2, '  FAIL  %s\n', name);
    end
end

function eqn(name, a, b)
    same = isequaln(a, b);
    ok(name, same);
    if ~same
        fprintf(2, '        (values differ)\n');
    end
end

function isa_(name, x, cls)
    ok(name, isa(x, cls));
end

function throws(name, id, fn)
    try
        fn();
    catch e
        ok(name, strcmp(e.identifier, id));
        if ~strcmp(e.identifier, id)
            fprintf(2, '        got id "%s"\n', e.identifier);
        end
        return;
    end
    ok(name, false);
    fprintf(2, '        %s did not throw\n', name);
end
