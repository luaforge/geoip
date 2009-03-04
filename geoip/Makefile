
# NOTE:
# To compile, you need the headers and the dynamic library of the GeoIP
# library.  They available in Debian in the package libgeoip-dev, or from the
# source distribution of that library, which can be downloaded here:
#
# http://geolite.maxmind.com/download/geoip/api/c/
#

LIBS	:=-lGeoIP
CFLAGS	:=-Wall

all: geoip.so

geoip.so: geoip.o
	$(CC) -shared -o $@ $^ $(LIBS)

