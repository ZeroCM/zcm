<a style="margin-right: 1rem;" href="javascript:history.go(-1)">Back</a>
[Home](../README.md)
# ZCM Type Grammar

This page describes the ZCM Type System and its grammar in very format terms. Unless you're interested in the
subtlties of the grammar, feel free to skim this document, and refer back as reference.

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

The grammar is given in an EBNF-style using regex-style repetition operators:

    file          = zcmtype*
    zcmtype       = 'struct' name '{' field* '}'
    field         = const_field | data_field
    const_field   = 'const' const_type name '=' const_literal ';'
    const_type    = 'int8_t' | 'int16_t' | 'int32_t' | 'int64_t' | 'float' | 'double'
    const_literal = hex_literal | integer | float_literal
    data_field    = type name arraydim* ';'
    type          = primative | name
    primative     = 'int8_t' | 'int16_t' | 'int32_t' | 'int64_t' | 'float' | 'double' | 'string' | 'boolean' | 'byte'
    int_type      = 'int8_t' | 'int16_t' | 'int32_t' | 'int64_t'
    arraydim      = '[' arraysize ']'
    arraysize     = name | unsigned_integer
    name          = underalpha underalphanum*
    underalpha    = [A-Za-z_]
    underalphanum = [A-Za-z0-9_]
    hex_literal   = "0x" | hexdigit+
    hexdigit      = [0-9A-Fa-f]

### Semantic Constraints

Using the grammar above, to be well-formed the following constraints must be satisfied:

  - Each field name must be unique
  - Names used for array sizes must refer to a field in the same 'zcmtype' that has a scalar 'int_type'
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

   - X is a normal data byte
   - L is a byte for a length field
   - N is a null byte

### Array Types

TODO

### Recursive Types

TODO

## Type Hashes

TODO

<hr>
<a style="margin-right: 1rem;" href="javascript:history.go(-1)">Back</a>
[Home](../README.md)
