#!/bin/sh

export temppath=$1
cd $temppath/rpm
export name=$2
export version=$3
export release=$4


TOP_DIR="/tmp/.rpm_create_"$2"_"`whoami`

LANG=C
export LANG

usage()
{
  echo "Usage:"
  echo "build-squid rpmdir packagename version release"
  exit 0
}

git_path="Unknown_path"
git_revision=$release
git_info()
{
  base=19000
  git_version=`git rev-list --all|wc -l`
  git_revision=`expr $git_version + $base`
  echo $git_revision
}

building_tag()
{
    git_revision=$release.`git log --pretty=oneline -1 |cut -c1-7`
    echo "new subversion is:"$version
}
building_tag
##git_info

echo "/etc/redhat-release:"
cat /etc/redhat-release
distroverpkg=$(awk -F= '$1=="distroverpkg"{print $2}' /etc/yum.conf)
releasever=$(rpm -q $distroverpkg|grep -Po "(?<=$distroverpkg-)[0-9]+")
if [[ -n "$releasever" ]]
then
        release="$git_revision".el$releasever
else
        release="$git_revision".el7
fi

RPM_MACROS=$HOME/.rpmmacros
if [ -e $RPM_MACROS ]; then
  mv -f $RPM_MACROS $RPM_MACROS.bak
fi


echo "%_topdir $TOP_DIR" > $RPM_MACROS
echo "%packager " `whoami` >> $RPM_MACROS
echo "%vendor TaoBao Inc." >> $RPM_MACROS
echo "%_git_path $git_path" >> $RPM_MACROS
echo "%_git_revision $git_revision" >> $RPM_MACROS
echo "%_release $release" >> $RPM_MACROS
echo "%debug_package %{nil}" >> $RPM_MACROS

cd ..
rm -rf $TOP_DIR
mkdir -p $TOP_DIR/RPMS
mkdir -p $TOP_DIR/SRPMS
mkdir -p $TOP_DIR/BUILD
mkdir -p $TOP_DIR/SOURCES
mkdir -p $TOP_DIR/SPECS

export fullname=$name-$version

ln -s . $fullname
tar --exclude=$fullname/$fullname \
    --exclude=$fullname/$fullname.tar.gz \
    -cf - $fullname/* | gzip -c9 >$fullname.tar.gz
cp $fullname.tar.gz $TOP_DIR/SOURCES

## remove temporary file or dir
rm $fullname.tar.gz
rm -rf $fullname

## create spec file from template
sed -e "s/_VERSION_/$version/g" -e "s/_RELEASE_/$release/g"  -e "s/SVN_REVISION/$git_revision/g" < rpm/$name.spec.in > $TOP_DIR/SPECS/$name.spec

rpmbuild --ba $TOP_DIR/SPECS/$name.spec

find $TOP_DIR/RPMS -name "*.rpm"  -exec mv {} ./rpm \;
echo "dir: $TOP_DIR"
rm -rf $TOP_DIR $RPM_MACROS
if [ -e $RPM_MACROS.bak ]; then
  mv -f $RPM_MACROS.bak $RPM_MACROS
fi

cd -
