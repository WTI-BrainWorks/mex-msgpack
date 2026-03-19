
function run_tests()
    tic;
    for i = 1:100
        test_roundtrip(1);
        test_roundtrip([1,2,5]);
        test_roundtrip(struct('a', 1, 'b', 'c'));
        test_roundtrip(struct('a', [1,5,6], 'b', 'foo'));
        test_roundtrip(struct('a', struct('a', 3, 'b', 500)));
        test_roundtrip({'foo', struct('thbar', {'300', '4'}, 'ah', [5 5])});
    end
    toc;
end

function test_roundtrip(obj)
    bytes = msgpack('pack', obj);
    obj2 = msgpack('unpack', bytes);
    %same = isequal(obj, obj2);
    %if ~same
    %    disp('not same:');
    %    disp(obj);
    %    disp(obj2);
    %end
end