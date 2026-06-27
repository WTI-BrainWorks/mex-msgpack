/*
Given there's a pre-built libmsgpack somewhere...

mex -I"./msgpack-c/include" msgpack_mex.c ./msgpack-c/libmsgpack-c.a -output msgpack

Uses only the classic mx* API, so it builds on everything from Octave 4.4 to
current MATLAB without -R2018a. See build.m for the full library + MEX build.
*/

#include "msgpack.h"
#include "mex.h"
#include <string.h>

void pack_mxarray(msgpack_packer* pk, const mxArray* arr);
mxArray* unpack_msgpack(msgpack_object obj);

void pack_mxarray(msgpack_packer* pk, const mxArray* arr) {
    if (!arr) {
        msgpack_pack_nil(pk);
        return;
    }

    // An empty array that isn't a string, cell, struct, or uint8 packs as nil,
    // so [] round-trips to [] (nil decodes to an empty double). This also gives
    // empty single/int arrays a sane encoding and fixes empty logical arrays,
    // which otherwise emitted no bytes at all. uint8 (our binary type) keeps an
    // empty bin, char an empty string, and cell an empty array -- each retains
    // its class on decode.
    if (mxGetNumberOfElements(arr) == 0 &&
        !mxIsChar(arr) && !mxIsCell(arr) && !mxIsStruct(arr) && !mxIsUint8(arr)) {
        msgpack_pack_nil(pk);
        return;
    }

    if (mxIsDouble(arr) && !mxIsComplex(arr)) {
        size_t num_elements = mxGetNumberOfElements(arr);

        if (num_elements == 1) {
            msgpack_pack_double(pk, mxGetScalar(arr));
        }
        else {
            double* vals = (double*)mxGetData(arr);
            msgpack_pack_array(pk, num_elements);
            for (size_t i = 0; i < num_elements; i++) {
                msgpack_pack_double(pk, (double)vals[i]);
            }
        }
    }
    else if (mxIsSingle(arr) && !mxIsComplex(arr)) {
        size_t num_elements = mxGetNumberOfElements(arr);
        float* vals = (float*)mxGetData(arr);

        if (num_elements != 1) {
            msgpack_pack_array(pk, num_elements);
        }
        for (size_t i = 0; i < num_elements; i++) {
            msgpack_pack_float(pk, vals[i]);
        }
    }
    else if (mxIsChar(arr)) {
        char* str = mxArrayToString(arr);
        if (str) {
            size_t len = strlen(str);
            msgpack_pack_str(pk, len);
            msgpack_pack_str_body(pk, str, len);
            mxFree(str);
        } else {
            // conversion failed (e.g. allocation failure); emit nil rather than
            // leaving a gap in the byte stream
            msgpack_pack_nil(pk);
        }
    }
    else if (mxIsCell(arr)) {
        size_t num_elements = mxGetNumberOfElements(arr);
        msgpack_pack_array(pk, num_elements);
        for (size_t i = 0; i < num_elements; i++) {
            pack_mxarray(pk, mxGetCell(arr, i));
        }
    }
    else if (mxIsStruct(arr)) {
        size_t num_elements = mxGetNumberOfElements(arr);
        int num_fields = mxGetNumberOfFields(arr);

        // A scalar struct packs as a bare map; any other count (including an
        // empty struct array) packs as an array of maps -- so a 0-element
        // struct array becomes an empty array instead of emitting no bytes.
        if (num_elements != 1) {
            msgpack_pack_array(pk, num_elements);
        }

        for (size_t e = 0; e < num_elements; e++) {
            msgpack_pack_map(pk, num_fields);
            for (int i = 0; i < num_fields; i++) {
                const char* field_name = mxGetFieldNameByNumber(arr, i);
                size_t len = strlen(field_name);
                msgpack_pack_str(pk, len);
                msgpack_pack_str_body(pk, field_name, len);
                pack_mxarray(pk, mxGetFieldByNumber(arr, e, i));
            }
        }
    }
    else if (mxIsLogical(arr)) {
        size_t num_elements = mxGetNumberOfElements(arr);
        mxLogical* logic_data = mxGetLogicals(arr);

        if (num_elements != 1) {
            msgpack_pack_array(pk, num_elements);
        }

        for (size_t i = 0; i < num_elements; i++) {
            if (logic_data[i]) {
                msgpack_pack_true(pk);
            } else {
                msgpack_pack_false(pk);
            }
        }
    }
    else if (mxIsUint8(arr)) {
        size_t num_elements = mxGetNumberOfElements(arr);
        msgpack_pack_bin(pk, num_elements);
        msgpack_pack_bin_body(pk, mxGetData(arr), num_elements);
    }
    else if (mxIsClass(arr, "int64") || mxIsClass(arr, "uint64") || 
             mxIsClass(arr, "int32") || mxIsClass(arr, "uint32") ||
             mxIsClass(arr, "int16") || mxIsClass(arr, "uint16") ||
             mxIsClass(arr, "int8")
            )
        {
        size_t num_elements = mxGetNumberOfElements(arr);
        mxClassID class_id = mxGetClassID(arr);
        void* data = mxGetData(arr);

        if (num_elements != 1) {
            msgpack_pack_array(pk, num_elements);
        }

        for (size_t i = 0; i < num_elements; i++) {
            if (class_id == mxINT64_CLASS) {
                msgpack_pack_int64(pk, ((int64_t*)data)[i]);
            }
            else if (class_id == mxUINT64_CLASS) {
                msgpack_pack_uint64(pk, ((uint64_t*)data)[i]);
            }
            else if (class_id == mxINT32_CLASS) {
                msgpack_pack_int32(pk, ((int32_t*)data)[i]);
            }
            else if (class_id == mxUINT32_CLASS) {
                msgpack_pack_uint32(pk, ((uint32_t*)data)[i]);
            }
            else if (class_id == mxINT16_CLASS) {
                msgpack_pack_int16(pk, ((int16_t*)data)[i]);
            }
            else if (class_id == mxUINT16_CLASS) {
                msgpack_pack_uint16(pk, ((uint16_t*)data)[i]);
            }
            else if (class_id == mxINT8_CLASS) {
                msgpack_pack_int8(pk, ((int8_t*)data)[i]);
            }
        }
    }
    else {
        mexErrMsgIdAndTxt("msgpack:pack:UnsupportedType", "Unsupported mxArray type.");
    }
}

mxArray* unpack_msgpack(msgpack_object obj) {
    switch (obj.type) {
        case MSGPACK_OBJECT_NIL:
            return mxCreateDoubleMatrix(0, 0, mxREAL);
        
        case MSGPACK_OBJECT_FLOAT:
        case MSGPACK_OBJECT_FLOAT32:
            return mxCreateDoubleScalar(obj.via.f64);
        
        case MSGPACK_OBJECT_STR: {
            char* temp_str = (char*)mxMalloc(obj.via.str.size + 1);
            memcpy(temp_str, obj.via.str.ptr, obj.via.str.size);
            temp_str[obj.via.str.size] = '\0';
            mxArray* mx_str = mxCreateString(temp_str);
            mxFree(temp_str);
            return mx_str;
        }
        
        case MSGPACK_OBJECT_ARRAY: {
            // TODO: handle booleans/integers
            uint32_t size = obj.via.array.size;
            if (!size) return mxCreateCellMatrix(1, 0);

            msgpack_object_type first_type = obj.via.array.ptr[0].type;
            if (first_type == MSGPACK_OBJECT_FLOAT ||
                first_type == MSGPACK_OBJECT_FLOAT32 ||
                first_type == MSGPACK_OBJECT_POSITIVE_INTEGER ||
                first_type == MSGPACK_OBJECT_NEGATIVE_INTEGER
            )
            {
                mxArray* dbl_arr = mxCreateDoubleMatrix(1, size, mxREAL);
                double* vals = (double*)mxGetData(dbl_arr);

                for (uint32_t i = 0; i < size; i++) {
                    msgpack_object elem = obj.via.array.ptr[i];
                    if (elem.type == MSGPACK_OBJECT_FLOAT ||
                        elem.type == MSGPACK_OBJECT_FLOAT32) {
                            vals[i] = elem.via.f64;
                    }
                    else if (elem.type == MSGPACK_OBJECT_POSITIVE_INTEGER) {
                        vals[i] = (double)elem.via.u64;
                    }
                    else if (elem.type == MSGPACK_OBJECT_NEGATIVE_INTEGER) {
                        vals[i] = (double)elem.via.i64;
                    }
                    else {
                        // bail and return a cell array
                        mxArray* fallback = mxCreateCellMatrix(1, size);
                        for (uint32_t j = 0; j < i; j++) {
                            mxSetCell(fallback, j, mxCreateDoubleScalar(vals[j]));
                        }
                        mxDestroyArray(dbl_arr);
                        for (uint32_t j = i; j < size; j++) {
                            mxSetCell(fallback, j, unpack_msgpack(obj.via.array.ptr[j]));
                        }
                        return fallback;
                    }
                }
                return dbl_arr;
            }

            // logical
            if (first_type == MSGPACK_OBJECT_BOOLEAN) {
                mxArray* bool_arr = mxCreateLogicalMatrix(1, size);
                mxLogical* vals = mxGetLogicals(bool_arr);

                for (uint32_t i = 0; i < size; i++) {
                    msgpack_object elem = obj.via.array.ptr[i];
                    if (elem.type == MSGPACK_OBJECT_BOOLEAN) {
                        vals[i] = elem.via.boolean ? true : false;
                    }
                    else {
                        // bail and return a cell array
                        mxArray* fallback = mxCreateCellMatrix(1, size);
                        for (uint32_t j = 0; j < i; j++) {
                            mxSetCell(fallback, j, mxCreateLogicalScalar(vals[j]));
                        }
                        mxDestroyArray(bool_arr);
                        for (uint32_t j = i; j < size; j++) {
                            mxSetCell(fallback, j, unpack_msgpack(obj.via.array.ptr[j]));
                        }
                        return fallback;
                    }
                }
                return bool_arr;
            }

            // struct array -- use the struct path only when the maps form a
            // uniform, string-keyed struct: the template (first) map's keys are all
            // strings of valid field-name length (<= 63), and every element is a map
            // with that exact same set of keys. Anything else (non-string keys,
            // over-long keys, or heterogeneous field sets) falls through to the
            // generic cell array (jsondecode-style) so we never read a non-string
            // key as a string, never hand an over-long name to mxCreateStructMatrix,
            // and never silently drop a field that isn't in the template.
            if (first_type == MSGPACK_OBJECT_MAP) {
                msgpack_object first_map = obj.via.array.ptr[0];
                uint32_t num_fields = first_map.via.map.size;

                int uniform = 1;
                for (uint32_t f = 0; f < num_fields; f++) {
                    msgpack_object key = first_map.via.map.ptr[f].key;
                    // valid MATLAB field names are 1..63 chars; anything else
                    // (non-string, empty, or over-long) -> cell fallback
                    if (key.type != MSGPACK_OBJECT_STR ||
                        key.via.str.size == 0 || key.via.str.size > 63) {
                        uniform = 0;
                        break;
                    }
                }

                if (uniform) {
                    const char** field_names = (const char**)mxMalloc(num_fields * sizeof(const char*));
                    for (uint32_t f = 0; f < num_fields; f++) {
                        msgpack_object key_obj = first_map.via.map.ptr[f].key;
                        char* fname = (char*)mxMalloc(key_obj.via.str.size + 1);
                        memcpy(fname, key_obj.via.str.ptr, key_obj.via.str.size);
                        fname[key_obj.via.str.size] = '\0';
                        field_names[f] = fname;
                    }

                    // every element must be a map whose keys are exactly the
                    // template's field set (order may differ); otherwise the array
                    // is heterogeneous and is returned as a cell of structs instead
                    for (uint32_t i = 0; i < size && uniform; i++) {
                        msgpack_object m = obj.via.array.ptr[i];
                        if (m.type != MSGPACK_OBJECT_MAP || m.via.map.size != num_fields) {
                            uniform = 0;
                            break;
                        }
                        for (uint32_t f = 0; f < num_fields; f++) {
                            msgpack_object key = m.via.map.ptr[f].key;
                            if (key.type != MSGPACK_OBJECT_STR) { uniform = 0; break; }
                            int found = 0;
                            for (uint32_t g = 0; g < num_fields; g++) {
                                if (key.via.str.size == strlen(field_names[g]) &&
                                    memcmp(key.via.str.ptr, field_names[g], key.via.str.size) == 0) {
                                    found = 1;
                                    break;
                                }
                            }
                            if (!found) { uniform = 0; break; }
                        }
                    }

                    if (uniform) {
                        mxArray* struct_arr = mxCreateStructMatrix(1, size, num_fields, field_names);
                        for (uint32_t i = 0; i < size; i++) {
                            msgpack_object map_obj = obj.via.array.ptr[i];
                            for (uint32_t f = 0; f < num_fields; f++) {
                                msgpack_object key_obj = map_obj.via.map.ptr[f].key;
                                msgpack_object val_obj = map_obj.via.map.ptr[f].val;

                                char temp_name[64] = {0};   // keys validated <= 63
                                memcpy(temp_name, key_obj.via.str.ptr, key_obj.via.str.size);

                                int field_num = mxGetFieldNumber(struct_arr, temp_name);
                                if (field_num >= 0) {
                                    mxSetFieldByNumber(struct_arr, i, field_num, unpack_msgpack(val_obj));
                                }
                            }
                        }
                        for (uint32_t f = 0; f < num_fields; f++) {
                            mxFree((void*)field_names[f]);
                        }
                        return struct_arr;
                    }

                    // heterogeneous after all -> release names and fall through
                    for (uint32_t f = 0; f < num_fields; f++) {
                        mxFree((void*)field_names[f]);
                    }
                    mxFree(field_names);
                }
            }

            // fallback to cell array
            mxArray* cell_arr = mxCreateCellMatrix(1, obj.via.array.size);
            for (uint32_t i = 0; i < obj.via.array.size; i++) {
                mxSetCell(cell_arr, i, unpack_msgpack(obj.via.array.ptr[i]));
            }
            return cell_arr;
        }

        case MSGPACK_OBJECT_MAP: {
            uint32_t map_size = obj.via.map.size;
            mxArray* struct_arr = mxCreateStructMatrix(1, 1, 0, NULL);
            
            for (uint32_t i = 0; i < map_size; i++) {
                msgpack_object key_obj = obj.via.map.ptr[i].key;
                msgpack_object val_obj = obj.via.map.ptr[i].val;

                if (key_obj.type != MSGPACK_OBJECT_STR) {
                    mxDestroyArray(struct_arr);
                    mexErrMsgIdAndTxt("msgpack:unpack:InvalidKey", "Struct fields must be strings.");
                }

                // MATLAB field names are capped at 63 chars; skip an over-long key
                // rather than depend on mxAddField's handling of an invalid name
                if (key_obj.via.str.size > 63) {
                    continue;
                }

                char* field_name = (char*)mxMalloc(key_obj.via.str.size + 1);
                memcpy(field_name, key_obj.via.str.ptr, key_obj.via.str.size);
                field_name[key_obj.via.str.size] = '\0';

                int field_num = mxAddField(struct_arr, field_name);
                if (field_num >= 0) {
                    mxSetFieldByNumber(struct_arr, 0, field_num, unpack_msgpack(val_obj));
                }
                mxFree(field_name);
            }
            return struct_arr;
        }

        case MSGPACK_OBJECT_BOOLEAN:
            return mxCreateLogicalScalar(obj.via.boolean);
        
        case MSGPACK_OBJECT_POSITIVE_INTEGER: {
            mxArray* uint_out = mxCreateNumericMatrix(1, 1, mxUINT64_CLASS, mxREAL);
            *((uint64_t*)mxGetData(uint_out)) = obj.via.u64;
            return uint_out;
        }

        case MSGPACK_OBJECT_NEGATIVE_INTEGER: {
            mxArray* int_out = mxCreateNumericMatrix(1, 1, mxINT64_CLASS, mxREAL);
            *((int64_t*)mxGetData(int_out)) = obj.via.i64;
            return int_out;
        }

        case MSGPACK_OBJECT_BIN: {
            mxArray* bin_arr = mxCreateNumericMatrix(1, obj.via.bin.size, mxUINT8_CLASS, mxREAL);
            memcpy(mxGetData(bin_arr), obj.via.bin.ptr, obj.via.bin.size);
            return bin_arr;
        }

        case MSGPACK_OBJECT_EXT: {
            const char* ext_fields[] = {"type", "data"};
            mxArray* ext_struct = mxCreateStructMatrix(1, 1, 2, ext_fields);
            mxArray* type_arr = mxCreateNumericMatrix(1, 1, mxINT8_CLASS, mxREAL);
            *((int8_t*)mxGetData(type_arr)) = obj.via.ext.type;
            mxSetFieldByNumber(ext_struct, 0, 0, type_arr);

            mxArray* data_arr = mxCreateNumericMatrix(1, obj.via.ext.size, mxUINT8_CLASS, mxREAL);
            memcpy(mxGetData(data_arr), obj.via.ext.ptr, obj.via.ext.size);
            mxSetFieldByNumber(ext_struct, 0, 1, data_arr);

            return ext_struct;
        }

        default:
            mexErrMsgIdAndTxt("msgpack:unpack:UnsupportedType", "Unsupported msgpack type.");
            return NULL;
    }
}

void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[]) {
    if (nrhs < 2) mexErrMsgIdAndTxt("msgpack:Usage", "Usage: msgpack('pack', data) or msgpack('unpack', bytes)");
    
    char* cmd = mxArrayToString(prhs[0]);
    if (!cmd) mexErrMsgIdAndTxt("msgpack:InvalidCommand", "First argument must be a string.");

    if (strcmp(cmd, "pack") == 0) {
        msgpack_sbuffer sbuf;
        msgpack_sbuffer_init(&sbuf);

        msgpack_packer pk;
        msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

        pack_mxarray(&pk, prhs[1]);
        plhs[0] = mxCreateNumericMatrix(1, sbuf.size, mxUINT8_CLASS, mxREAL);
        memcpy(mxGetData(plhs[0]), sbuf.data, sbuf.size);
        msgpack_sbuffer_destroy(&sbuf);
    }

    else if (strcmp(cmd, "unpack") == 0) {
        if (!mxIsUint8(prhs[1])) {
            mxFree(cmd);
            mexErrMsgIdAndTxt("msgpack:InvalidInput", "Unpack requires a uint8 array.");
        }

        const char* data = (const char*)mxGetData(prhs[1]);
        size_t size = mxGetNumberOfElements(prhs[1]);

        msgpack_unpacked msg;
        msgpack_unpacked_init(&msg);

        // Only SUCCESS means a complete object was parsed. The old `if (ret)` test
        // wrongly treated PARSE_ERROR (-1) and NOMEM_ERROR (-2) as success and fed
        // garbage into unpack_msgpack; CONTINUE (0, truncated input) was the only
        // value it rejected. With off == NULL the library returns SUCCESS even when
        // trailing bytes follow the first object, so trailing data is still accepted
        // and the first value returned.
        if (msgpack_unpack_next(&msg, data, size, NULL) == MSGPACK_UNPACK_SUCCESS) {
            plhs[0] = unpack_msgpack(msg.data);
        } else {
            msgpack_unpacked_destroy(&msg);
            mxFree(cmd);
            mexErrMsgIdAndTxt("msgpack:UnpackFailed", "Failed to parse MessagePack data.");
        }

        msgpack_unpacked_destroy(&msg);
    }
    else {
        mxFree(cmd);
        mexErrMsgIdAndTxt("msgpack:InvalidCommand", "Unknown command. Use 'pack' or 'unpack'.");
    }

    mxFree(cmd);
}
