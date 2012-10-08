PKGNAME="gettext"
PKGVER="0.18.1.1"

SOURCE=http://ftp.gnu.org/pub/gnu/${PKGNAME}/${PKGNAME}-${PKGVER}.tar.gz
SOURCE2=http://mingw-w64-dgn.googlecode.com/svn/trunk/patch/gettext-0.18.x-w64.patch

pushd ../source
wget -c $SOURCE $SOURCE2
popd 

tar xzf ../source/${PKGNAME}-${PKGVER}.tar.gz
cd ${PKGNAME}-${PKGVER}

patch -p0 < ../../source/gettext-0.18.x-w64.patch
./configure --prefix=/mingw/local
make -j 4
make install
