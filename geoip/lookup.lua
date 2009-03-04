#! /usr/bin/env lua
-- vim:sw=4:sts=4

if #arg == 0 then
    print "Arg: ip address(s) or hostname(s)"
    os.exit(1)
end

geoip = require "geoip"
g = geoip.open_type("city", "country")

for _, name in ipairs(arg) do
    r = g:lookup(name)
    print(name, r)
end

