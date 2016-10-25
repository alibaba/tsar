use TestLua;

plan tests => 2 * blocks();

run_tests();

__DATA__

=== TEST 1: empty tables as objects
--- lua
local cjson = require "cjson"
print(cjson.encode({}))
print(cjson.encode({dogs = {}}))
--- out
{}
{"dogs":{}}



=== TEST 2: empty tables as arrays
--- lua
local cjson = require "cjson"
cjson.encode_empty_table_as_object(false)
print(cjson.encode({}))
print(cjson.encode({dogs = {}}))
--- out
[]
{"dogs":[]}



=== TEST 3: empty tables as objects (explicit)
--- lua
local cjson = require "cjson"
cjson.encode_empty_table_as_object(true)
print(cjson.encode({}))
print(cjson.encode({dogs = {}}))
--- out
{}
{"dogs":{}}



=== TEST 4: empty_array userdata
--- lua
local cjson = require "cjson"
print(cjson.encode({arr = cjson.empty_array}))
--- out
{"arr":[]}



=== TEST 5: empty_array_mt
--- lua
local cjson = require "cjson"
local empty_arr = setmetatable({}, cjson.empty_array_mt)
print(cjson.encode({arr = empty_arr}))
--- out
{"arr":[]}



=== TEST 6: empty_array_mt and empty tables as objects (explicit)
--- lua
local cjson = require "cjson"
local empty_arr = setmetatable({}, cjson.empty_array_mt)
print(cjson.encode({obj = {}, arr = empty_arr}))
--- out
{"arr":[],"obj":{}}



=== TEST 7: empty_array_mt and empty tables as objects (explicit)
--- lua
local cjson = require "cjson"
cjson.encode_empty_table_as_object(true)
local empty_arr = setmetatable({}, cjson.empty_array_mt)
local data = {
  arr = empty_arr,
  foo = {
    obj = {},
    foobar = {
      arr = cjson.empty_array,
      obj = {}
    }
  }
}
print(cjson.encode(data))
--- out
{"foo":{"foobar":{"obj":{},"arr":[]},"obj":{}},"arr":[]}



=== TEST 8: empty_array_mt on non-empty tables
--- lua
local cjson = require "cjson"
cjson.encode_empty_table_as_object(true)
local array = {"hello", "world", "lua"}
setmetatable(array, cjson.empty_array_mt)
local data = {
  arr = array,
  foo = {
    obj = {},
    foobar = {
      arr = cjson.empty_array,
      obj = {}
    }
  }
}
print(cjson.encode(data))
--- out
{"foo":{"foobar":{"obj":{},"arr":[]},"obj":{}},"arr":["hello","world","lua"]}



=== TEST 9: & in JSON
--- lua
local cjson = require "cjson"
local a="[\"a=1&b=2\"]"
local b=cjson.decode(a)
print(cjson.encode(b))
--- out
["a=1&b=2"]



=== TEST 10: default and max precision
--- lua
local math = require "math"
local cjson = require "cjson"
local double = math.pow(2, 53)
print(cjson.encode(double))
cjson.encode_number_precision(16)
print(cjson.encode(double))
print(string.format("%16.0f", cjson.decode("9007199254740992")))
--- out
9.007199254741e+15
9007199254740992
9007199254740992
