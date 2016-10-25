Name
====

lua-cjson - Fast JSON encoding/parsing

Table of Contents
=================

* [Name](#name)
* [Description](#description)
* [Additions to mpx/lua](#additions)
    * [encode_empty_table_as_object](#encode_empty_table_as_object)
    * [empty_array](#empty_array)
    * [empty_array_mt](#empty_array_mt)
    * [encode_number_precision](#encode_number_precision)

Description
===========

This fork of [mpx/lua-cjson](https://github.com/mpx/lua-cjson) is included in
the [OpenResty](https://openresty.org/) bundle and includes a few bugfixes and
improvements, especially to facilitate the encoding of empty tables as JSON Arrays.

Please refer to the [lua-cjson documentation](http://www.kyne.com.au/~mark/software/lua-cjson.php)
for standard usage, this README only provides informations regarding this fork's additions.

See [`mpx/master..openresty/master`](https://github.com/mpx/lua-cjson/compare/master...openresty:master)
for the complete history of changes.

[Back to TOC](#table-of-contents)

Additions
=========

encode_empty_table_as_object
----------------------------
**syntax:** `cjson.encode_empty_table_as_object(true|false|"on"|"off")`

Change the default behavior when encoding an empty Lua table.

By default, empty Lua tables are encoded as empty JSON Objects (`{}`). If this is set to false,
empty Lua tables will be encoded as empty JSON Arrays instead (`[]`).

This method either accepts a boolean or a string (`"on"`, `"off"`).

[Back to TOC](#table-of-contents)

empty_array
-----------
**syntax:** `cjson.empty_array`

A lightuserdata, similar to `cjson.null`, which will be encoded as an empty JSON Array by
`cjson.encode()`.

For example, since `encode_empty_table_as_object` is `true` by default:

```lua
local cjson = require "cjson"

local json = cjson.encode({
    foo = "bar",
    some_object = {},
    some_array = cjson.empty_array
})
```

This will generate:

```json
{
    "foo": "bar",
    "some_object": {},
    "some_array": []
}
```

[Back to TOC](#table-of-contents)

empty_array_mt
--------------
**syntax:** `setmetatable({}, cjson.empty_array_mt)`

A metatable which can "tag" a table as a JSON Array in case it is empty (that is, if the
table has no elements, `cjson.encode()` will encode it as an empty JSON Array).

Instead of:

```lua
local function serialize(arr)
    if #arr < 1 then
        arr = cjson.empty_array
    end

    return cjson.encode({some_array = arr})
end
```

This is more concise:

```lua
local function serialize(arr)
    setmetatable(arr, cjson.empty_array_mt)

    return cjson.encode({some_array = arr})
end
```

Both will generate:

```json
{
    "some_array": []
}
```

[Back to TOC](#table-of-contents)

encode_number_precision
-----------------------
**syntax:** `cjson.encode_number_precision(precision)`

This fork allows encoding of numbers with a `precision` up to 16 decimals (vs. 14 in mpx/lua-cjson).

[Back to TOC](#table-of-contents)
