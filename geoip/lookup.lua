#! /usr/bin/env lua
-- vim:sw=4:sts=4

if #arg ~= 1 then
    print "Arg: ip address or hostname"
    os.exit(1)
end

geoip = require("geoip")

g = geoip.open_type "city"
r = g:lookup(arg[1])
print(r)

