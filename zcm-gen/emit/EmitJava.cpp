#include "Common.hpp"
#include "GetOpt.hpp"
#include "ZCMGen.hpp"
#include "Emitter.hpp"
#include "util/StringUtil.hpp"
#include "util/FileUtil.hpp"

// #include <stdio.h>
// #include <stdint.h>
// #include <stdlib.h>
// #include <string.h>
// #ifdef WIN32
// #define __STDC_FORMAT_MACROS
// #endif
// #include <inttypes.h>
// #include <assert.h>

// #include <sys/stat.h>
// #include <sys/types.h>

// #include "zcmgen.h"

// #include "getopt.h"

// #ifdef WIN32
// #include <zcm/windows/WinPorting.h>
// #endif


static string dotsToSlashes(const string& s)
{
    return StringUtil::replace(s, '.', '/');
}

static void makeDirsForFile(const string& path)
{
    string dir = FileUtil::dirname(path);
    FileUtil::mkdirWithParents(dir, 0755);
}

void setupOptionsJava(GetOpt& gopt)
{
    gopt.addString(0,   "jpath",     "",         "Java file destination directory");
    gopt.addBool(0,    "jmkdir",     1,         "Make java source directories automatically");
    gopt.addString(0,   "jdecl",      "implements zcm.zcm.ZCMEncodable", "String added to class declarations");
    gopt.addString(0,   "jdefaultpkg", "zcmtypes", "Default Java package if ZCM type has no package");
}

struct PrimInfo
{
    string storage;
    string decode;
    string encode;

    PrimInfo(const string& storage, const string& decode, const string& encode) :
        storage(storage), decode(decode), encode(encode) {}
};

static bool jdefaultpkgWarned = false;

string makeFqn(ZCMGen& zcm, const string& typeName)
{
    if (typeName.find('.') != string::npos)
        return typeName;

    if (!jdefaultpkgWarned && !zcm.gopt->wasSpecified("jdefaultpkg")) {
        printf("Notice: enclosing ZCM types without package into java namespace '%s'.\n",
               zcm.gopt->getString("jdefaultpkg").c_str());
        jdefaultpkgWarned = true;
    }

    return zcm.gopt->getString("jdefaultpkg") + "." + typeName;
}

// /** # -> replace1
//     @ -> replace2
// **/
static string specialReplace(const string& haystack, const string& replace1)
{
    string ret;
    for (auto& c : haystack) {
        if (c == '#')
            ret += replace1;
        else
            ret += string(1, c);
    }
    return ret;
}

static string makeAccessor(ZCMMember& lm, const string& obj)
{
    string ret;
    ret += obj + (obj.size() == 0 ? "" : ".") + lm.membername;
    for (size_t d = 0 ; d < lm.dimensions.size(); d++) {
        char buf[10];
        sprintf(buf,"[%c]", 'a'+(int)d);
        ret += buf;
    }
    return ret;
}

/** Make an accessor that points to the last array **/
static string makeAccessorArray(ZCMMember& lm, const string& obj)
{
    string ret;
    ret += obj + (obj.size() == 0 ? "" : ".") + lm.membername;
    for (size_t d = 0 ; d < lm.dimensions.size()-1; d++) {
        char buf[10];
        sprintf(buf,"[%c]", 'a'+(int)d);
        ret += buf;
    }
    return ret;
}

static bool structHasStringMember(ZCMStruct& lr)
{
    for (auto& lm : lr.members)
        if (lm.type.fullname == "string")
            return true;
    return false;
}

static string dimSizePrefix(const string& dimSize)
{
    char *eptr;
    strtol(dimSize.c_str(), &eptr, 0);
    if(*eptr == '\0')
        return "";
    else
        return "this.";
}

struct TypeTable
{
    std::unordered_map<string, PrimInfo> tbl;

    TypeTable()
    {
        tbl.emplace("byte", PrimInfo{
            "byte",
            "# = ins.readByte();",
            "outs.writeByte(#);"});

        tbl.emplace("int8_t", PrimInfo{
            "byte",
            "# = ins.readByte();",
            "outs.writeByte(#);"});

        tbl.emplace("int16_t", PrimInfo{
            "short",
            "# = ins.readShort();",
            "outs.writeShort(#);"});

        tbl.emplace("int32_t", PrimInfo{
            "int",
            "# = ins.readInt();",
            "outs.writeInt(#);"});

        tbl.emplace("int64_t", PrimInfo{
            "long",
            "# = ins.readLong();",
            "outs.writeLong(#);"});

        tbl.emplace("string", PrimInfo{
            "String",
            "__strbuf = new char[ins.readInt()-1]; for (int _i = 0; _i < __strbuf.length; _i++) __strbuf[_i] = (char) (ins.readByte()&0xff); ins.readByte(); # = new String(__strbuf);",
            "__strbuf = new char[#.length()]; #.getChars(0, #.length(), __strbuf, 0); outs.writeInt(__strbuf.length+1); for (int _i = 0; _i < __strbuf.length; _i++) outs.write(__strbuf[_i]); outs.writeByte(0);"});

        tbl.emplace("boolean", PrimInfo{
            "boolean",
            "# = ins.readByte()!=0;",
            "outs.writeByte( # ? 1 : 0);"});

        tbl.emplace("float", PrimInfo{
            "float",
            "# = ins.readFloat();",
            "outs.writeFloat(#);"});

        tbl.emplace("double", PrimInfo{
            "double",
            "# = ins.readDouble();",
            "outs.writeDouble(#);"});
    }

    PrimInfo *find(const string& type)
    {
        auto it = tbl.find(type);
        if (it == tbl.end())
            return nullptr;
        else
            return &it->second;
    }
};

static TypeTable typeTable;

struct EmitStruct : public Emitter
{
    ZCMGen& zcm;
    ZCMStruct& lr;

    EmitStruct(ZCMGen& zcm, ZCMStruct& lr, const string& fname):
        Emitter(fname), zcm(zcm), lr(lr) {}

    void encodeRecursive(ZCMMember& lm, PrimInfo *pinfo, const string& accessor, int depth)
    {
        int ndims = (int)lm.dimensions.size();

        // base case: primitive array
        if (depth+1 == ndims && pinfo != nullptr) {
            string accessorArray = makeAccessorArray(lm, "");
            if (pinfo->storage == "byte") {
                auto& dim = lm.dimensions[depth];
                if (dim.mode == ZCM_VAR) {
                    emit(2+depth, "if (this.%s > 0)", dim.size.c_str());
                    emit(3+depth, "outs.write(this.%s, 0, %s);", accessorArray.c_str(), dim.size.c_str());
                } else {
                    emit(2+depth, "outs.write(this.%s, 0, %s);", accessorArray.c_str(), dim.size.c_str());
                }
                return;
            }
        }

        // base case: generic
        if (depth == ndims) {
            emitStart(2 + ndims, "");
            if (pinfo != NULL)
                emitContinue("%s", specialReplace(pinfo->encode, accessor).c_str());
            else
                emitContinue("%s", specialReplace("#._encodeRecursive(outs);", accessor).c_str());
            emitEnd(" ");

            return;
        }

        auto& dim = lm.dimensions[depth];
        emit(2+depth, "for (int %c = 0; %c < %s%s; %c++) {",
             'a'+depth, 'a'+depth, dimSizePrefix(dim.size).c_str(), dim.size.c_str(), 'a'+depth);

        encodeRecursive(lm, pinfo, accessor, depth+1);

        emit(2+depth, "}");
    }

    void decodeRecursive(ZCMMember& lm, PrimInfo *pinfo, const string& accessor, int depth)
    {
        int ndims = (int)lm.dimensions.size();

        // base case: primitive array
        if (depth+1 == ndims && pinfo != nullptr) {
            string accessorArray = makeAccessorArray(lm, "");

            // byte array
            if (pinfo->storage == "byte") {
                auto& dim = lm.dimensions[depth];
                emitStart(2+depth, "ins.readFully(this.%s, 0, %s);", accessorArray.c_str(), dim.size.c_str());
                return;
            }
        }

        // base case: generic
        if (depth == ndims) {
            emitStart(2 + ndims,"");
            if (pinfo)
                emitContinue("%s", specialReplace(pinfo->decode, accessor).c_str());
            else {
                emitContinue("%s = %s._decodeRecursiveFactory(ins);", accessor.c_str(), makeFqn(zcm, lm.type.fullname).c_str());
            }
            emitEnd("");
            return;
        }

        auto& dim = lm.dimensions[depth];
        emit(2+depth, "for (int %c = 0; %c < %s%s; %c++) {",
             'a'+depth, 'a'+depth, dimSizePrefix(dim.size).c_str(), dim.size.c_str(), 'a'+depth);

        decodeRecursive(lm, pinfo, accessor, depth+1);

        emit(2+depth, "}");
    }

    void copyRecursive(ZCMMember& lm, PrimInfo *pinfo, const string& accessor, int depth)
    {
        int ndims = (int)lm.dimensions.size();

        // base case: primitive array
        if (depth+1 == ndims && pinfo != nullptr) {
            string accessorArray = makeAccessorArray(lm, "");

            // one method works for all primitive types, yay!
            auto& dim = lm.dimensions[depth];

            if (dim.mode == ZCM_VAR) {
                emit(2+depth, "if (this.%s > 0)", dim.size.c_str());
                emitStart(3+depth, "System.arraycopy(this.%s, 0, outobj.%s, 0, %s%s);",
                          accessorArray.c_str(),
                          accessorArray.c_str(),
                          dimSizePrefix(dim.size).c_str(),
                          dim.size.c_str());
            } else {
                emitStart(2+depth, "System.arraycopy(this.%s, 0, outobj.%s, 0, %s%s);",
                          accessorArray.c_str(),
                          accessorArray.c_str(),
                          dimSizePrefix(dim.size).c_str(),
                          dim.size.c_str());
            }

            return;
        }

        // base case: generic
        if (depth == ndims) {
            if (pinfo) {

                emitStart(2+ndims, "outobj.%s", lm.membername.c_str());
                for (int i = 0; i < ndims; i++)
                    emitContinue("[%c]", 'a'+i);

                emitContinue(" = this.%s", lm.membername.c_str());
                for (int i = 0; i < ndims; i++)
                    emitContinue("[%c]", 'a'+i);

                emitEnd(";");

            } else {
                emit(2+depth, "outobj.%s = this.%s.copy();", accessor.c_str(), accessor.c_str());
            }

            return;
        }

        auto& dim = lm.dimensions[depth];
        emit(2+depth, "for (int %c = 0; %c < %s%s; %c++) {",
             'a'+depth, 'a'+depth, dimSizePrefix(dim.size).c_str(), dim.size.c_str(), 'a'+depth);

        copyRecursive(lm, pinfo, accessor, depth+1);

        emit(2+depth, "}");
    }

    void emitStruct()
    {
        emit(0, "/* ZCM type definition class file");
        emit(0, " * This file was automatically generated by zcm-gen");
        emit(0, " * DO NOT MODIFY BY HAND!!!!");
        emit(0, " */");
        emit(0, "");

        if (lr.structname.package.size() > 0)
            emit(0, "package %s;", lr.structname.package.c_str());
        else
            emit(0, "package %s;", zcm.gopt->getString("jdefaultpkg").c_str());

        emit(0, " ");
        emit(0, "import java.io.*;");
        emit(0, "import java.util.*;");
        emit(0, "import zcm.zcm.*;");
        emit(0, " ");
        emit(0, "public final class %s %s", lr.structname.shortname.c_str(), zcm.gopt->getString("jdecl").c_str());
        emit(0, "{");

        for (auto& lm : lr.members) {
            PrimInfo *pinfo = typeTable.find(lm.type.fullname);
            emitStart(1, "public ");

            if (pinfo)  {
                emitContinue("%s", pinfo->storage.c_str());
            } else {
                emitContinue("%s", makeFqn(zcm, lm.type.fullname).c_str());
            }
            emitContinue(" %s", lm.membername.c_str());
            for (size_t i = 0; i < lm.dimensions.size(); i++)
                emitContinue("[]");
            emitEnd(";");
        }
        emit(0," ");

        // public constructor
        emit(1,"public %s()", lr.structname.shortname.c_str());
        emit(1,"{");

        // pre-allocate any fixed-size arrays.
        for (auto& lm : lr.members) {
            PrimInfo *pinfo = typeTable.find(lm.type.fullname);

            int ndims = (int)lm.dimensions.size();
            if (ndims == 0 || lm.isConstantSizeArray())
                continue;

            emitStart(2, "%s = new ", lm.membername.c_str());
            if (pinfo)
                emitContinue("%s", pinfo->storage.c_str());
            else
                emitContinue("%s", makeFqn(zcm, lm.type.fullname).c_str());

            for (auto& dim : lm.dimensions)
                emitContinue("[%s]", dim.size.c_str());
            emitEnd(";");
        }
        emit(1,"}");
        emit(0," ");

        emit(1, "public static final long ZCM_FINGERPRINT;");
        emit(1, "public static final long ZCM_FINGERPRINT_BASE = 0x%016" PRIx64 "L;", lr.hash);
        emit(0," ");

        //////////////////////////////////////////////////////////////
        // CONSTANTS
        for (auto& lc : lr.constants) {
            assert(ZCMGen::isLegalConstType(lc.type));

            auto& tn = lc.type;
            auto *name = lc.membername.c_str();
            auto *value = lc.valstr.c_str();

            if (tn == "int8_t") {
                emit(1, "public static final byte %s = (byte) %s;", name, value);
            } else if (tn == "int16_t") {
                emit(1, "public static final short %s = (short) %s;", name, value);
            } else if (tn == "int32_t") {
                emit(1, "public static final int %s = %s;", name, value);
            } else if (tn == "int64_t") {
                emit(1, "public static final long %s = %sL;", name, value);
            } else if (tn == "float") {
                emit(1, "public static final float %s = %sf;", name, value);
            } else if (tn == "double") {
                emit(1, "public static final double %s = %s;", name, value);
            } else {
                assert(0);
            }
        }
        if (lr.constants.size() > 0)
            emit(0, "");

        ///////////////// compute fingerprint //////////////////
        emit(1, "static {");
        emit(2, "ZCM_FINGERPRINT = _hashRecursive(new ArrayList<Class<?>>());");
        emit(1, "}");
        emit(0, " ");

        emit(1, "public static long _hashRecursive(ArrayList<Class<?>> classes)");
        emit(1, "{");
        emit(2, "if (classes.contains(%s.class))", makeFqn(zcm, lr.structname.fullname).c_str());
        emit(3,     "return 0L;");
        emit(0, " ");
        emit(2, "classes.add(%s.class);", makeFqn(zcm, lr.structname.fullname).c_str());

        emit(2, "long hash = ZCM_FINGERPRINT_BASE");
        for (auto& lm : lr.members) {
            PrimInfo *pinfo = typeTable.find(lm.type.fullname);
            if (pinfo)
                continue;
            emit(3, " + %s._hashRecursive(classes)", makeFqn(zcm, lm.type.fullname).c_str());
        }
        emit(3,";");

        emit(2, "classes.remove(classes.size() - 1);");
        emit(2, "return (hash<<1) + ((hash>>63)&1);");

        emit(1, "}");
        emit(0, " ");

        ///////////////// encode //////////////////

        emit(1,"public void encode(DataOutput outs) throws IOException");
        emit(1,"{");
        emit(2,"outs.writeLong(ZCM_FINGERPRINT);");
        emit(2,"_encodeRecursive(outs);");
        emit(1,"}");
        emit(0," ");

        emit(1,"public void _encodeRecursive(DataOutput outs) throws IOException");
        emit(1,"{");
        if (structHasStringMember(lr))
            emit(2, "char[] __strbuf = null;");

        for (auto& lm : lr.members) {
            PrimInfo *pinfo = typeTable.find(lm.type.fullname);
            string accessor = makeAccessor(lm, "this");
            encodeRecursive(lm, pinfo, accessor, 0);
            emit(0," ");
        }
        emit(1,"}");
        emit(0," ");

        ///////////////// decode //////////////////
        auto *sn = lr.structname.shortname.c_str();
        auto fqn_ = makeFqn(zcm, lr.structname.fullname);
        auto *fqn = fqn_.c_str();

        // decoding constructors
        emit(1, "public %s(byte[] data) throws IOException", sn);
        emit(1, "{");
        emit(2, "this(new ZCMDataInputStream(data));");
        emit(1, "}");
        emit(0, " ");
        emit(1,"public %s(DataInput ins) throws IOException", sn);
        emit(1,"{");
        emit(2,"if (ins.readLong() != ZCM_FINGERPRINT)");
        emit(3,     "throw new IOException(\"ZCM Decode error: bad fingerprint\");");
        emit(0," ");
        emit(2,"_decodeRecursive(ins);");
        emit(1,"}");
        emit(0," ");

        emit(1,"public static %s _decodeRecursiveFactory(DataInput ins) throws IOException", fqn);
        emit(1,"{");
        emit(2,"%s o = new %s();", fqn, fqn);
        emit(2,"o._decodeRecursive(ins);");
        emit(2,"return o;");
        emit(1,"}");
        emit(0," ");

        emit(1,"public void _decodeRecursive(DataInput ins) throws IOException");
        emit(1,"{");
        if (structHasStringMember(lr))
            emit(2, "char[] __strbuf = null;");

        for (auto& lm : lr.members) {
            PrimInfo *pinfo = typeTable.find(lm.type.fullname);
            string accessor = makeAccessor(lm, "this");

            // allocate an array if necessary
            if (lm.dimensions.size() > 0) {

                emitStart(2, "this.%s = new ", lm.membername.c_str());

                if (pinfo)
                    emitContinue("%s", pinfo->storage.c_str());
                else
                    emitContinue("%s", makeFqn(zcm, lm.type.fullname).c_str());

                for (auto& dim : lm.dimensions)
                    emitContinue("[(int) %s]", dim.size.c_str());
                emitEnd(";");
            }

            decodeRecursive(lm, pinfo, accessor, 0);
            emit(0," ");
        }

        emit(1,"}");
        emit(0," ");


        ///////////////// copy //////////////////
        string classname = makeFqn(zcm, lr.structname.fullname);
        emit(1,"public %s copy()", classname.c_str());
        emit(1,"{");
        emit(2,"%s outobj = new %s();", classname.c_str(), classname.c_str());

        for (auto& lm : lr.members) {
            PrimInfo *pinfo = typeTable.find(lm.type.fullname);
            string accessor = makeAccessor(lm, "");

            // allocate an array if necessary
            if (lm.dimensions.size() > 0) {

                emitStart(2, "outobj.%s = new ", lm.membername.c_str());

                if (pinfo)
                    emitContinue("%s", pinfo->storage.c_str());
                else
                    emitContinue("%s", makeFqn(zcm, lm.type.fullname).c_str());

                for (auto& dim : lm.dimensions)
                    emitContinue("[(int) %s]", dim.size.c_str());
                emitEnd(";");
            }

            copyRecursive(lm, pinfo, accessor, 0);
            emit(0," ");
        }

        emit(2,"return outobj;");
        emit(1,"}");
        emit(0," ");

        ////////
        emit(0, "}\n");
    }
};

int emitJava(ZCMGen& zcm)
{
    string jpath = zcm.gopt->getString("jpath");
    string jpathPrefix = jpath + (jpath.size() > 0 ? "/" : "");
    bool jmkdir = zcm.gopt->getBool("jmkdir");

    //////////////////////////////////////////////////////////////
    // STRUCTS
    for (auto& lr : zcm.structs) {
        string classname = makeFqn(zcm, lr.structname.fullname);
        string path = jpathPrefix + dotsToSlashes(classname);

        if (!zcm.needsGeneration(lr.zcmfile, path))
            continue;

        if (jmkdir)
            makeDirsForFile(path);

        EmitStruct E{zcm, lr, path};
        if (!E.good()) {
            printf("E.good() failed!\n");
            return -1;
        }
        E.emitStruct();
    }

    return 0;
}
