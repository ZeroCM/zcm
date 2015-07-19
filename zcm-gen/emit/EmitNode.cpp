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

static int primtypeSize(const string& type)
{
    if (type == "double")  return 8;
    if (type == "float")   return 4;
    if (type == "int64_t") return 8;
    if (type == "int32_t") return 4;
    if (type == "int16_t") return 2;
    if (type == "int8_t")  return 1;
    if (type == "boolean") return 1;

    return 0;
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
        emit(0, "    var buf = data;");
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
        emit(0, "function createWriter(size) {");
        emit(0, "    var buf = new Buffer(size);");
        emit(0, "    var offset = 0;");
        emit(0, "    var methods = {");
        emit(0, "        writeDouble: function(value) {");
        emit(0, "            buf.writeDoubleBE(value, offset);");
        emit(0, "            offset += 8;");
        emit(0, "        },");
        emit(0, "        writeFloat: function(value) {");
        emit(0, "            buf.writeFloatBE(value, offset);");
        emit(0, "            offset += 4;");
        emit(0, "        },");
        emit(0, "        write64: function(value) {");
        emit(0, "            ref.writeInt64BE(buf, offset, value);");
        emit(0, "            offset += 8;");
        emit(0, "        },");
        emit(0, "        write32: function(value) {");
        emit(0, "            buf.writeInt32BE(value, offset);");
        emit(0, "            offset += 4;");
        emit(0, "        },");
        emit(0, "        write16: function(value) {");
        emit(0, "            buf.writeInt16BE(value, offset);");
        emit(0, "            offset += 2;");
        emit(0, "        },");
        emit(0, "        write8: function(value) {");
        emit(0, "            buf.writeInt8(value, offset);");
        emit(0, "            offset += 1;");
        emit(0, "        },");
        emit(0, "        writeBoolean: function(value) {");
        emit(0, "            buf.writeInt8(value, offset);");
        emit(0, "            offset += 1;");
        emit(0, "        },");
        emit(0, "        writeString: function(value) {");
        emit(0, "            methods.write32(value.length+1);");
        emit(0, "            ref.writeCString(buf, offset, value);");
        emit(0, "            offset += value.length+1;");
        emit(0, "        },");
        emit(0, "        getBuffer: function() {");
        emit(0, "            return buf;");
        emit(0, "        },");
        emit(0, "    };");
        emit(0, "    return methods;");
        emit(0, "}");
        emit(0, "");
    }

    void emitEncodedSizeBody(int indent, ZCMStruct& ls)
    {
        // XXX: this is wrong!
        //emit(indent, "return 1024*1024;");
        emit(indent, "return 1024;");
    }

    void emitEncodePrimType(int indent, const string& rvalue, const string& type)
    {
        if (type == "double") {
            emit(indent, "W.writeDouble(%s);", rvalue.c_str());
        } else if (type == "float") {
            emit(indent, "W.writeFloat(%s);", rvalue.c_str());
        } else if (type == "int64_t") {
            emit(indent, "W.write64(%s);", rvalue.c_str());
        } else if (type == "int32_t") {
            emit(indent, "W.write32(%s);", rvalue.c_str());
        } else if (type == "int16_t") {
            emit(indent, "W.write16(%s);", rvalue.c_str());
        } else if (type == "int8_t") {
            emit(indent, "W.write8(%s);", rvalue.c_str());
        } else if (type == "boolean") {
             emit(indent, "W.writeBoolean(%s);", rvalue.c_str());
        } else if (type == "string") {
             emit(indent, "W.writeString(%s);", rvalue.c_str());
        } else {
            fprintf(stderr, "Unimpl type '%s' for rvalue='%s'\n", type.c_str(), rvalue.c_str());
        }
    }

    void emitEncodeBody(int indent, ZCMStruct& ls)
    {
        emit(indent, "var size = %s.encodedSize(msg);", ls.structname.shortname.c_str());
        emit(indent, "var W = createWriter(size);");
        // XXX compute and emit the correct hash
        emit(indent, "W.write64(123456789);");
        for (auto& lm : ls.members) {
            auto& mtn = lm.type.fullname;
            auto& mn = lm.membername;
            int ndims = (int)lm.dimensions.size();
            if (ndims == 0) {
                emitEncodePrimType(indent, "msg."+mn, mtn);
            } else {
                // For-loop open-braces
                char v = 'a';
                for (int i = 0; i < ndims; i++) {
                    auto sz = dimSizeAccessor(lm.dimensions[i].size);
                    emit(indent+i, "for (var %c = 0; %c < %s; %c++) {", v, v, sz.c_str(), v);
                    string array = (i == 0) ? "msg."+mn : string(1, v-1)+"elt";
                    emit(indent+i+1, "var %celt = %s[%c];", v, array.c_str(), v);
                    v++;
                };

                // For-loop body
                emitEncodePrimType(indent+ndims, string(1, v-1)+"elt", mtn);

                // For-loop close-braces
                for (int i = ndims-1; i >= 0; i--)
                    emit(indent+i, "}");
            }
        }
        emit(indent, "return W.getBuffer();");
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
        auto *sn = ls.structname.shortname.c_str();
        emit(0, "var %s = {", sn);
        emit(0, "    encodedSize: function(msg) {");
        emitEncodedSizeBody(2, ls);
        emit(0, "    },");
        emit(0, "    encode: function(msg) {");
        emitEncodeBody(2, ls);
        emit(0, "    },");
        emit(0, "    decode: function(data) {");
        emitDecodeBody(2, ls);
        emit(0, "    }");
        emit(0, "}");
        emit(0, "exports.%s = %s;", sn, sn);
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
