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

svn_path="Unknown_path"
svn_revision="1"
svn_info()
{
  str=`svn info ../ 2>/dev/null |
  awk -F': ' '{if($1=="URL") print $2}'`

  if [ -z "$str" ]; then return; fi

  svn_path=$str
  str=`svn info ../ 2>/dev/null |
  awk -F': ' '{if($1=="Last Changed Rev") print $2}'`

  if [ -z "$str" ]; then
    echo "!! Please upgrade your subversion: sudo yum install subversion"
    return;
  fi

  svn_revision=$str
}

svn_info

if [ `cat /etc/redhat-release|cut -d " " -f 7|cut -d "." -f 1` = 4 ]
then
        release="$svn_revision".el4
elif [ `cat /etc/redhat-release|cut -d " " -f 7|cut -d "." -f 1` = 5 ]
then
        release="$svn_revision".el5
elif [ `cat /etc/redhat-release|cut -d " " -f 7|cut -d "." -f 1` = 6 ]
then
        release="$svn_revision".el6
else
        release="$svn_revision".el5
fi

RPM_MACROS=$HOME/.rpmmacros
if [ -e $RPM_MACROS ]; then
  mv -f $RPM_MACROS $RPM_MACROS.bak
fi


echo "%_topdir $TOP_DIR" > $RPM_MACROS
echo "%packager " `whoami` >> $RPM_MACROS
echo "%vendor TaoBao Inc." >> $RPM_MACROS
echo "%_svn_path $svn_path" >> $RPM_MACROS
echo "%_svn_revision $svn_revision" >> $RPM_MACROS
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
tar --exclude=$fullname/*/.svn \
    --exclude=$fullname/$fullname \
    --exclude=$fullname/$fullname.tar.gz \
    -cf - $fullname/* | gzip -c9 >$fullname.tar.gz
cp $fullname.tar.gz $TOP_DIR/SOURCES

## remove temporary file or dir
rm $fullname.tar.gz
rm -rf $fullname

## create spec file from template
sed -e "s/_VERSION_/$version/g" -e "s/_RELEASE_/$release/g"  -e "s/SVN_REVISION/$svn_revision/g" < rpm/$name.spec.in > $TOP_DIR/SPECS/$name.spec

rpmbuild --ba $TOP_DIR/SPECS/$name.spec

find $TOP_DIR/RPMS -name "*.rpm"  -exec mv {} ./rpm \;

rm -rf $TOP_DIR $RPM_MACROS
if [ -e $RPM_MACROS.bak ]; then
  mv -f $RPM_MACROS.bak $RPM_MACROS
fi

cd -
