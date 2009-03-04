-- vim:sw=4:sts=4:filetype=lua

package = "GeoIP"
version = "0.1-1"

source = {
    url = "file:///home/wolfgang/devel/lua/geoip/geoip-0.1-1.tar.gz"
}

description = {
    summary = "Binding to MaxMind's libGeoIP",
    detailed = [[not yet]],
    homepage = "http://geoip.luaforge.net/",
    license = "LGPL",
}

dependencies = {
    "lua >= 5.1",
}

external_dependencies = {
    GEOIP = {
	header = "GeoIP.h",
    }
}

build = {
    type = "module",
    modules = {
	geoip = {
	    sources = { "geoip.c" },
	    libraries = { "GeoIP" },
	}
    }
}

