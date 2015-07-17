#include "Common.hpp"
#include "GetOpt.hpp"
#include "util/StringUtil.hpp"
#include "ZCMGen.hpp"
#include "Emitter.hpp"

void setupOptionsNode(GetOpt& gopt)
{
}

static string dimSizeAccessor(const string& dimSize)
{
    assert(dimSize.size() > 0);
    char c = dimSize[0];
    if ('0' <= c && c <= '9')
        return dimSize;
    else
        return "msg."+dimSize;
}

struct EmitModule : public Emitter
{
    ZCMGen& zcm;

    EmitModule(ZCMGen& zcm, const string& fname):
        Emitter(fname), zcm(zcm) {}

    void emitHeader()
    {
        emit(0, "console.log('Hello from zcmtypes!');");
        emit(0, "var ref = require('ref');");
        emit(0, "");
        emit(0, "function createReader(data) {");
        emit(0, "    var buf = new Buffer(data);");
        emit(0, "    var offset = 0;");
        emit(0, "    var methods = {");
        emit(0, "        readDouble: function() {");
        emit(0, "            var ret = buf.readDoubleBE(offset);");
        emit(0, "            offset += 8;");
        emit(0, "            return ret;");
        emit(0, "        },");
        emit(0, "        readFloat: function() {");
        emit(0, "            var ret = buf.readFloatBE(offset);");
        emit(0, "            offset += 4;");
        emit(0, "            return ret;");
        emit(0, "        },");
        emit(0, "        read64: function() {");
        emit(0, "            var ret = ref.readInt64BE(buf, offset);");
        emit(0, "            offset += 8;");
        emit(0, "            return ret;");
        emit(0, "        },");
        emit(0, "        read32: function() {");
        emit(0, "            var ret = buf.readInt32BE(offset);");
        emit(0, "            offset += 4;");
        emit(0, "            return ret;");
        emit(0, "        },");
        emit(0, "        read16: function() {");
        emit(0, "            var ret = buf.readInt16BE(offset);");
        emit(0, "            offset += 2;");
        emit(0, "            return ret;");
        emit(0, "        },");
        emit(0, "        read8: function() {");
        emit(0, "            var ret = buf.readInt8(offset);");
        emit(0, "            offset += 1;");
        emit(0, "            return ret;");
        emit(0, "        },");
        emit(0, "        readBoolean: function() {");
        emit(0, "            var ret = buf.readInt8(offset);");
        emit(0, "            offset += 1;");
        emit(0, "            return ret != 0;");
        emit(0, "        },");
        emit(0, "        readString: function() {");
        emit(0, "            var len = methods.read32();");
        emit(0, "            var ret = ref.readCString(buf, offset);");
        emit(0, "            offset += len;");
        emit(0, "            return ret;");
        emit(0, "        },");
        emit(0, "    };");
        emit(0, "    return methods;");
        emit(0, "}");
        emit(0, "");
    }


    void emitEncodeBody(int indent, ZCMStruct& ls)
    {
    }

    void emitDecodePrimType(int indent, const string& lvalue, const string& type)
    {
        if (type == "double") {
            emit(indent, "%s = R.readDouble();", lvalue.c_str());
        } else if (type == "float") {
            emit(indent, "%s = R.readFloat();", lvalue.c_str());
        } else if (type == "int64_t") {
            emit(indent, "%s = R.read64();", lvalue.c_str());
        } else if (type == "int32_t") {
            emit(indent, "%s = R.read32();", lvalue.c_str());
        } else if (type == "int16_t") {
            emit(indent, "%s = R.read16();", lvalue.c_str());
        } else if (type == "int8_t") {
            emit(indent, "%s = R.read8();", lvalue.c_str());
        } else if (type == "boolean") {
             emit(indent, "%s = R.readBoolean();", lvalue.c_str());
        } else if (type == "string") {
             emit(indent, "%s = R.readString();", lvalue.c_str());
        } else {
            fprintf(stderr, "Unimpl type '%s' for lvalue='%s'\n", type.c_str(), lvalue.c_str());
        }
    }

    void emitDecodeBody(int indent, ZCMStruct& ls)
    {
        emit(indent, "var R = createReader(data);");
        emit(indent, "var hash = R.read64();");
        emit(indent, "var msg = {}");
        emit(indent, "msg.__type = '%s';", ls.structname.shortname.c_str());
        for (auto& lm : ls.members) {
            auto& mtn = lm.type.fullname;
            auto& mn = lm.membername;
            int ndims = (int)lm.dimensions.size();
            if (ndims == 0) {
                emitDecodePrimType(indent, "msg."+mn, mtn);
            } else {
                emit(indent, "msg.%s = [];", mn.c_str());

                // For-loop open-braces
                char v = '_';
                for (int i = 0; i < ndims; i++) {
                    v = 'a'+i;
                    auto sz = dimSizeAccessor(lm.dimensions[i].size);
                    emit(indent+i, "for (var %c = 0; %c < %s; %c++) {", v, v, sz.c_str(), v);
                    if (i != ndims-1)
                        emit(indent+i+1, "var %celt = [];", v);
                };

                // For-loop bodies
                emitDecodePrimType(indent+ndims, "var "+string(1, v)+"elt", mtn);

                // For-loop close-braces
                for (int i = ndims-1; i >= 0; i--) {
                    char nextv = 'a'+i-1;
                    string array = (i == 0) ? "msg."+mn : string(1, nextv)+"elt";
                    emit(indent+i, "    %s.push(%celt);", array.c_str(), v);
                    emit(indent+i, "}");
                    v = nextv;
                }
            }
        }

        // XXX: we need to validate the hash!
        //emit(indent, "console.log(timestamp, position0, position1, position2, orient0, orient1, orient2, orient3);");
        emit(indent, "return msg;");
    }

    void emitStruct(ZCMStruct& ls)
    {
        // XXX: should we be using fullname here?
        emit(0, "exports.%s = {", ls.structname.shortname.c_str());
        emit(0, "    encode: function(msg) {");
        emitEncodeBody(2, ls);
        emit(0, "    },");
        emit(0, "    decode: function(data) {");
        emitDecodeBody(2, ls);
        emit(0, "    }");
        emit(0, "}");
    }

    void emitModule()
    {
        emitHeader();
        for (auto& ls : zcm.structs) {
            emitStruct(ls);
        }
    }
};

int emitNode(ZCMGen& zcm)
{
    EmitModule E{zcm, "zcmtypes.js"};
    if (!E.good())
        return -1;

    E.emitModule();

    return 0;
}
