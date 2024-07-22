#!/bin/bash


set -e 

NAME=notes-editor
VERSION=`git describe --tags`
VERSION="${VERSION:1}" # Remove initial 'v'
DATE_NOW=`date -R`

echo "Building $NAME version $VERSION"

mv debian/changelog debian/changelog.old
echo "$NAME ($VERSION) unstable; urgency=medium" > debian/changelog
echo -e "\n  * Change goes here\n" >> debian/changelog
echo -e " -- Jens Olsson <jens@rootsy.nu>  $DATE_NOW" >> debian/changelog

echo "" >> debian/changelog
cat debian/changelog.old >> debian/changelog
rm debian/changelog.old

vim debian/changelog

sed -i "s/Version: .*/Version: $VERSION/" $NAME/DEBIAN/control

mkdir -p ${NAME}/usr/bin
sudo objcopy --strip-debug --strip-unneeded ../build/src/$NAME $NAME/usr/bin/$NAME

sudo mkdir -p $NAME/usr/share/man/man1
gzip -9 -n -c  < debian/manpage.1  > $NAME.1.gz
sudo mv $NAME.1.gz $NAME/usr/share/man/man1

gzip -9 -n -c  < debian/changelog  > changelog.gz
sudo mv changelog.gz $NAME/usr/share/doc/$NAME
sudo chown -R root:root $NAME/usr

dpkg-deb --build $NAME
lintian $NAME.deb

mv $NAME.deb ${NAME}_${VERSION}_amd64.deb

OWNER=$USER
sudo chown -R "$OWNER" .
