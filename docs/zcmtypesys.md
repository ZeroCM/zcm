<a style="margin-right: 1rem;" href="javascript:history.go(-1)">Back</a>
[Home](../README.md)
# ZCM Type System
<h4><i>Note: There has been a recent change to the zcm hashing system.
       Check out the announcement <a href=announcements/hash_scheme_change.html>here</a></i></h4>

This page describes the ZCM Type System grammar, encoding, and type hashes in very formal terms. Unless you're
intimately concerned with the subtlties, feel free to skim this document, and refer back as reference.

## Grammar

### Primitives

<table>
    <tr><td>  int8_t   </td><td>  8-bit signed integer              </td></tr>
    <tr><td>  int16_t  </td><td>  16-bit signed integer             </td></tr>
    <tr><td>  int32_t  </td><td>  32-bit signed integer             </td></tr>
    <tr><td>  int64_t  </td><td>  64-bit signed integer             </td></tr>
    <tr><td>  float    </td><td>  32-bit IEEE floating point value  </td></tr>
    <tr><td>  double   </td><td>  64-bit IEEE floating point value  </td></tr>
    <tr><td>  string   </td><td>  UTF-8 string                      </td></tr>
    <tr><td>  boolean  </td><td>  true/false logical value          </td></tr>
    <tr><td>  byte     </td><td>  8-bit value                       </td></tr>
</table>

### Specification

The grammar is given in EBNF using regex-style repetition and character classes:

    file          = zcmtype*
    zcmtype       = 'struct' name '{' field* '}'
    field         = const_field | data_field
    const_field   = 'const' const_type name '=' const_literal ';'
    const_type    = 'int8_t' | 'int16_t' | 'int32_t' | 'int64_t' | 'float' | 'double'
    const_literal = hex_literal | int_literal | float_literal
    data_field    = type name arraydim* ';'
    type          = primative | name
    primative     = 'int8_t' | 'int16_t' | 'int32_t' | 'int64_t' | 'float' | 'double' | 'string' | 'boolean' | 'byte'
    int_type      = 'int8_t' | 'int16_t' | 'int32_t' | 'int64_t'
    arraydim      = '[' arraysize ']'
    arraysize     = name | uint_literal
    name          = underalpha underalphanum*
    underalpha    = [A-Za-z_]
    underalphanum = [A-Za-z0-9_]
    hex_literal   = "0x" | hexdigit+
    hexdigit      = [0-9A-Fa-f]
    uint_literal  = [0-9]+
    int_literal   = '-' uint_literal

### Semantic Constraints

Using the grammar above, to be well-formed the following constraints must be satisfied:

  - Each field's *name* must be unique
  - Names used for array sizes must refer to a field in the same 'zcmtype' that has a scalar integer type
  - Names used for 'type' refer to other 'zcmtype' definitions
    - These may exist in other files

## Encoding formats

### Primitives

<table>
    <thead>
    <tr><th>  Type     </th><th> Encoded Size </th><th> Format</th></tr>
    </thead>
    <tr><td>  int8_t   </td><td> 1 byte         </td><td>  X              </td></tr>
    <tr><td>  int8_t   </td><td> 1 byte         </td><td>  X              </td></tr>
    <tr><td>  int16_t  </td><td> 2 bytes        </td><td>  XX             </td></tr>
    <tr><td>  int32_t  </td><td> 4 bytes        </td><td>  XXXX           </td></tr>
    <tr><td>  int64_t  </td><td> 8 bytes        </td><td>  XXXXXXXX       </td></tr>
    <tr><td>  float    </td><td> 4 bytes        </td><td>  XXXX           </td></tr>
    <tr><td>  double   </td><td> 8 bytes        </td><td>  XXXXXXXX       </td></tr>
    <tr><td>  string   </td><td> 4+len+1 bytes  </td><td>  LLLL&lt;chars&gt;N   </td></tr>
    <tr><td>  boolean  </td><td> 1 byte         </td><td>  X              </td></tr>
    <tr><td>  byte     </td><td> 1 byte         </td><td>  X              </td></tr>
</table>

 Where:

   - X is a data byte
   - L is a length byte
   - N is a null byte

### Array Types

Array types are encoded as a simple series of the element type. The econding does *NOT* include a length field for the
the dimensions. For static array dimensions, the size is already known by the decoder. For dynamic array dimensions, the
size is encoded in another field (as mandated by the grammar). For these reasons, there is zero encoding overhead for
arrays. This includes nested types.

### Recursive/Nested Types

Nested types are also encoded with zero overhead. Since the decoder knows the layout, there is no reason to encode
type metadata.

## Type Hashes

The optimized encoding formats specified above are made possible using a type hash. Each encoded message starts with
a 64-bit hash field. As seen above, for one message, this is the only size overhead in ZCM Type encodings. Without the
hash, the encoded data is at maximum the same size as an equivalent C struct. Further, the hash is a unique type identifier.
The hash allows a decoder function to verify that a binary blob of data is ecoded as expected.

To acheive this lofty goal, it is crucial to get the type hash computation right. We must ensure that that a hash unique
identifies a type layout. The hash is not intended to be cryptographic, but instead to catch programming and configuration
errors.

Hashing primatives:

    int64 hashbyte(i64 hash, byte v)
    {
        return ((hash<<8) ^ (hash>>53)) + v;
    }

    int64 hashstring(int64 hash, string s)
    {
        hashbyte(s.length);
        for (b in s)
            hashbyte(b);
    }

Hashing zcmtypes:

    int64 hashtype()
    {
        int64 hash = 0x12345678;

        if (HASH_TYPENAME)
            hash = hashstring(hash, zcmtype_name);

        for (fld in fields) {
            if (HASH_MEMBER_NAMES)
                hash = hashstring(hash, fld.name);

            // Hash the type (only if its a primative)
            if (isPrimativeType(fld.typename))
                hash = hashstring(hash, fld.typename);

            // Hash the array dimmensionality
            hash = hashbyte(hash, fld.numdims)
            for (dim in fld.dimlist) {
                hash = hashbyte(hash, dim.mode);   // static (0) or dynamic (1)
                hash = hashstring(hash, dim.size); // the text btwn [] from the .zcm file
            }
        }
    }

The hashing function above works well, but an observent reader will quickly notice that it completely
ignores nested zcmtypes. This is done because zcmtypes may be defined in different files and thus, the
type generator may not have access to their definitions. To resolve this, ZCM defers the final
hash computation until runtime, when it can use all dependent types.

The final hash computation will be triggered on a type's first runtime use and will recurse into nested
types as needed. The hash code computed above in *hashtype()* is typically called the *base hash*
because it's used as the starting point in the recursive-nested hash computation. The recursive computation
is fairly simple. The algorithm proceeds as follows:

    i64 TYPE_hash_recursive()
    {
        i64 hash = BASE_HASH;
                 + SUBTYPE1_hash_recursive()
                 + SUBTYPE2_hash_recursive()
                 + SUBTYPE3_hash_recursive()
                 ...;

        return ROTL(hash, 1); // rotate left by 1
    }

<hr>
 <a style="margin-right: 1rem;" href="javascript:history.go(-1)">Back</a>
[Home](../README.md)
