
# NOTE:
# To compile, you need the headers of the GeoIP library.  They are included
# in the source distribution of this library, which can be downloaded here:
#
# http://geolite.maxmind.com/download/geoip/api/c/
#
# Also, either the shared or static library must be available.  This is either
# already installed on your system (e.g. Debian: libgeoip1) or the result
# from compiling the library.  For the shared library, you may have to
# create a symbolic link named "libGeoIP.so" in /usr/bin.
#


LIBS	:=-lGeoIP
CFLAGS	:=-Wall -I GeoIP-1.4.6/libGeoIP

all: geoip.so

geoip.so: geoip.o
	$(CC) -shared -o $@ $^ $(LIBS)

