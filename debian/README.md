# Building debian packages in a PPA

## Preparation

### Install packages
```
apt-get install devscripts debhelper build-essential
```

### Get the orig tarball
```
curl -fLO https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.6.21.tar.gz
mv linux-6.6.21.tar.gz ../linux-premier_6.6.21.orig.tar.gz
```


## Create new release

### Update kernel config

After changing Kconfig files or updating the configuration in debian/config/annotations
you'll need to make sure the configuration is in sync with the kernel's Kconfig system.
```
debian/rules updateconfigs
git add debian/config/annotations
git commit -sm 'UBUNTU: [Config] Update for ..'
```

### Update debian/reconstruct

The source package is split into the .orig.tar.gz tarball plus a big patch on
top. Unfortunately the patch doesn't preserve file permissions of added files,
so the debian/reconstruct script is needed to fix those after applying the
patch. If any new files with execute permission (scripts) are added we need update
the debian/reconstruct file like so:
```
debian/rules autoreconstruct
git add debian/reconstruct
```

### Update debian/changelog

The source package name and version is all controlled by the debian/changelog
file. It has a certain format and is most easily updated with the following
command:
```
dch -i
```

The version number is usually of the form 6.6.21-42.1 eg.
`<upstream version>-<abi>.<upload>`. Different kernels can be installed
simultaneously as long as they have different upstream version and/or ABI
numbers. The upload number is used if fx. one release failed to build in the
PPA. Then you can fix it and bump just the upload number without bumping the
ABI release number.

Make sure to change the release codename from `UNRELEASED` and commit the
updated changelog (and possibly debian/reconstruct). Usually the commit
message should contain the new version number in the changelog. Eg.:
```
git add debian/changelog
git commit -sm 'UBUNTU: linux-premier-6.6.21-42.1'
```

### Build source package

Make sure you have a clean git repository
```
git clean -fdx
```
and then build the source package with
```
debian/rules build-sources
```

This builds a source package that just refers to the .orig.tar.gz, but doesn't
include it. If you update the base kernel version or upload to a new PPA that
doesn't already have the .orig.tar.gz you'll need to include it in the source
package like so:
```
debian/rules build-sources-with-orig
```

### Push the source package to a PPA

For the PPA to accept the upload the source package needs to be signed by
a user that has push access to the PPA:
```
debsign ../linux-premier_6.6.21-42.1_source.changes
```

Finally you can upload it to your PPA with
```
dput <ppa> ../linux-premier_6.6.21-42.1_source.changes
```
Here `<ppa>` of course needs to be defined in your `~/.dput.cf`.
