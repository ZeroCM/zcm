<a style="margin-right: 1rem;" href="javascript:history.go(-1)">Back</a>
[Home](../README.md)
# ZCM Type System

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
    const_type    = int_type numbits? | float_type | 'byte'
    const_literal = hex_literal | int_literal | float_literal
    data_field    = type name arraydim* ';'
    type          = primative | name
    primative     = int_type numbits? | float_type | 'string' | 'boolean' | 'byte'
    int_type      = 'int8_t' | 'int16_t' | 'int32_t' | 'int64_t'
    float_type    = 'float' | 'double'
    numbits       = ':' int_literal
    arraydim      = '[' arraysize ']'
    arraysize     = name | uint_literal
    name          = underalpha underalphanum*
    underalpha    = [A-Za-z_]
    underalphanum = [A-Za-z0-9_]
    hex_literal   = "0x" | hexdigit+
    hexdigit      = [0-9A-Fa-f]
    uint_literal  = [0-9]+
    int_literal   = '-'? uint_literal

### Semantic Constraints

Using the grammar above, to be well-formed the following constraints must be satisfied:

  - Each field's *name* must be unique
  - Names used for array sizes must refer to a field in the same 'zcmtype' that has a scalar integer type
  - Names used for 'type' refer to other 'zcmtype' definitions
    - These may exist in other files
  - The absolute value of the int_literal specified for numbits must always be less
    than the number of bits of the corresponding type
  - Sign extension of bitfields via negative numbits are not allowed on `byte` type.
    See the bitfields section below for how sign extension works.

## Encoding formats

Note that if your machine architecture does not natively support `int8_t` and `uint8_t`
types, signed zcmtype members may not decode negative numbers properly. Similarly sign
extension on bitfields may also not function properly. These are known issues and if you need
them addressed, please create an issue on
[zcm's github issue page](https://github.com/ZeroCM/zcm/issues).

### Primitives

<table>
    <thead>
    <tr><th>  Type     </th><th> Encoded Size </th><th> Format</th></tr>
    </thead>
    <tr><td>  int8_t     </td><td> 1 byte                   </td><td>  X                  </td></tr>
    <tr><td>  int16_t    </td><td> 2 bytes                  </td><td>  XX                 </td></tr>
    <tr><td>  int32_t    </td><td> 4 bytes                  </td><td>  XXXX               </td></tr>
    <tr><td>  int64_t    </td><td> 8 bytes                  </td><td>  XXXXXXXX           </td></tr>
    <tr><td>  float      </td><td> 4 bytes                  </td><td>  XXXX               </td></tr>
    <tr><td>  double     </td><td> 8 bytes                  </td><td>  XXXXXXXX           </td></tr>
    <tr><td>  string     </td><td> 4+len+1 bytes            </td><td>  LLLL&lt;chars&gt;N </td></tr>
    <tr><td>  boolean    </td><td> 1 byte                   </td><td>  X                  </td></tr>
    <tr><td>  byte       </td><td> 1 byte                   </td><td>  X                  </td></tr>
    <tr><td>  _bitfield_ </td><td> bitpacked with neighbors </td><td>  |+                 </td></tr>
</table>

 Where:

   - X is a data byte
   - L is a length byte
   - N is a null byte
   - | is an individual bit

### Bitfields

Bitfields are integer types with a specified number of bits that are bitpacked during encoding.
Neighboring bitfields will be packed tightly, wasting no bits in between (not necessarily
maintaining byte alignment). This is unlike all other type encodings which maintain byte alignment.
Bitfields currently only support big endian encoding. All bitfields will behave exactly like
their non-bitfield type in all regards other than encoding and decoding. Sign extension is
configurable by specifying a negative sign before the number of bits in the bitfield. When
encoding a type that contains an `int8_t:3` with the value set to `0b111`, you should expect the
decoded message to contain a `7` as the value of this variable. A type with an `int8_t:-3` with
the value set to `0b111` will have its sign extended upon decode. You should expect the received
value to be `-1` (`0b11111111`).

`byte` is unsigned for any language that supports unsigned types. When encoding a type that
contains a `byte:3` with the value set to `0b111`, you should expect the decoded message to
contain a `7` as the value of this variable for languages that support unsigned types.
For languages that do not support unsigned types (ahem java...) you should still expect the
decoded message to contain a `7` as the value of this variable. However, for a type containing
a `byte:8` with the value set to `0xff`, you should expect the decoded message to contain a
`255` for languages that support unsigned types and a `-1` for languages that do not.


### Array Types

Array types are encoded as a simple series of the element type. The encoding does *NOT* include a length field for the
dimensions. For static array dimensions, the size is already known by the decoder. For dynamic array dimensions, the
size is encoded in another field (as mandated by the grammar). For these reasons, there is zero encoding overhead for
arrays. This includes nested types.

### Recursive/Nested Types

Nested types are also encoded with zero overhead. Since the decoder knows the layout, there is no reason to encode
type metadata. Circular type dependencies are not currently supported.

## Type Hashes
####*Note: Announcement on membername hashing found [here](announcements/hash_scheme_change.md)*

The optimized encoding formats specified above are made possible using a type hash. Each encoded message starts with
a 64-bit hash field. As seen above, for one message, this is the only size overhead in ZCM Type encodings. Without the
hash, the encoded data is at maximum the same size as an equivalent C struct. Further, the hash is a unique type identifier.
The hash allows a decoder function to verify that a binary blob of data is encoded as expected.

To acheive this lofty goal, it is crucial to get the type hash computation right. We must ensure that that a hash uniquely
identifies a type layout. The hash is not intended to be cryptographic, but instead to catch programming and configuration
errors.

Hashing primatives:

    i64 hashbyte(i64 hash, byte v)
    {
        return ((((u64)hash)<<8) ^ (((u64)hash)>>53)) + v;
    }

    i64 hashstring(i64 hash, string s)
    {
        hashbyte(s.length);
        for (b in s)
            hashbyte(b);
    }

Hashing zcmtypes:

    i64 hashtype()
    {
        i64 hash = 0x12345678;

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
        u64 hash = BASE_HASH;
                 + SUBTYPE1_hash_recursive()
                 + SUBTYPE2_hash_recursive()
                 + SUBTYPE3_hash_recursive()
                 ...;

        return ROTL(hash, 1); // rotate left by 1
    }

## Packages

Zcmgen allows the user to specify the package of the zcmtype which will then be used on a
language-by-language bases to group types into namespaces, modules, etc. The semantics for
specifying the package are as shown in the example below, which constructs a type `bar`
within the package `foo`. Note that the specified package can actually be multiple nested
packages, ie replacing `foo` with `foo1.foo2` would instead place the type `bar` within
the package `foo2` which itself is within the package `foo1`.

    package foo;
    struct bar {
        baz  b;
        .qux q;
    };

When a type belongs to a package, all nonprimitive types within that type are assumed to
also be from that package. In the example above, the zcmtype `foo.bar` contains a member
`b` of type `foo.baz` (ie the package `foo` is automatically prepended to the specified
type `baz` because the zcmtype `bar` is from the package `foo`). Should the user wish
to specify a type that does not belong to the same package as the containing type, they
can prepend the type with a `.` as in the case of the member `q` from the example, which
will not belong to any package. This also allows the user to specify a member type from a
completely separate package by prepending a leading `.` before the package. For instance,
if the zcmtype `qux` actually belonged to a package `quuz` (that is not part of `foo`),
replacing `.qux` with `.quuz.qux` would properly specify the desired type.

Note also that although some languages allow unqualified access to types from
parent packages, the zcmtype specification does not. Specifically, for the following
2 types, note that `t2` must specify its `t1` member as existing within the package
`.foo` even though `t2` itself exists within a child package of `foo`.

    package foo;
    struct t1 {
        int8_t a;
    };

    package foo.bar;
    struct t2 {
        .foo.t1 b;
    };


<hr>
 <a style="margin-right: 1rem;" href="javascript:history.go(-1)">Back</a>
[Home](../README.md)
