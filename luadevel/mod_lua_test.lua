local _M = {
    _VERSION = "0.1.0"
}

local cjson = require("cjson")
local socket = require("socket")
local http = require("socket.http")
local ltn12 = require("ltn12")
local DETAIL_BIT = tsar.DETAIL_BIT
local SUMMARY_BIT = tsar.SUMMARY_BIT
local HIDE_BIT = tsar.HIDE_BIT
local STATS_NULL = tsar.STATS_NULL
local string_format = string.format

local info = {
    {hdr = " value1", summary_bit = SUMMARY_BIT, merge_mode = 0, stats_opt = STATS_NULL},
    {hdr = " value2", summary_bit = DETAIL_BIT, merge_mode = 0, stats_opt = STATS_NULL},
    {hdr = " value3", summary_bit = DETAIL_BIT, merge_mode = 0, stats_opt = STATS_NULL},
}

local value1 = 1
local value2 = 2
local value3 = 3
function _M.read(mod, param)
    value1 = value1 + 3
    value2 = value2 + 3
    value3 = value3 + 3

    return string_format("%d,%d,%d", value1, value2, value3)
end

local totalvalue = 0
function _M.set(mod, st_array, pre_array, cur_array, interval)
    totalvalue = totalvalue + cur_array[2]

    st_array[1] = cur_array[1] - pre_array[1]
    st_array[2] = (cur_array[1] - pre_array[1]) * 100.0 / totalvalue
    st_array[3] = cur_array[3]

    return st_array, pre_array, cur_array
end

function _M.register()
    return {
        opt = "--test",
        usage = "test information",
        info = info,
    }
end

return _M
