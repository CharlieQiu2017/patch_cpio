# patch_cpio: Erasing non-reproducible build information from `cpio` archives

In building and publishing open source software it is often desirable to make the build [*reproducible*](https://reproducible-builds.org/),
meaning any public auditor can follow the same steps used to build the package,
and obtain an output that is byte-for-byte identical to the published version.
This requires eliminating every single bit of information contained in the output that depends on uncontrollable environment factors,
such as build time, hostname, or filesystem quirks.

The last step of building a package is usually *archiving*,
that is packaging all output files into a single file.
In the Unix world two archiving formats are in common use: `tar` and `cpio`.
While `tar` is no doubt the more popular one,
certain environments still require using `cpio`.
For example, the Linux kernel [initramfs](https://www.kernel.org/doc/Documentation/filesystems/ramfs-rootfs-initramfs.txt) must be built as a `cpio` archive.

The `tar` manual specifically has a [page](https://www.gnu.org/software/tar/manual/html_node/Reproducibility.html) about making archive files reproducible.
However, there is no corresponding guide for `cpio`.
Some techniques that apply to `tar` archives may not apply to `cpio` since it supports much less command flags than `tar`.

Here we provide a gadget `patch_cpio` that patches `cpio` archives in-place to eliminate unreproducible build information from them.

## Building

Prerequisite: Linux, C++11 compiler (`gcc` or `clang`).

* Invoke `make` to build the release version.
* Inovke `make debug=1` to build the debug version.

## Usage

`patch_cpio [cpio_file]`

The input `cpio` file must be in the `newc` format.

To create a reproducible archive of a (non-root, non-empty) directory `dir`, use the following command:

```bash
cd dir
find . -print0 | tail -z -n +2 | LC_ALL=C sort -z | (cpio -o -0 -H newc > ../archive.cpio)
patch_cpio ../archive.cpio
```

We first list the directory content using `find`.
The first entry `find` emits is usually just `.` itself which should be removed.
The remaining entries are sorted to ensure deterministic order of files.
We add `LC_ALL=C` to eliminate any influence of locale on sorting.
Finally, the list of files is piped to `cpio` for archiving.
The output is patched using our gadget.

## Implementation

This [document](https://github.com/libyal/dtformats/blob/main/documentation/Copy%20in%20and%20out%20(CPIO)%20archive%20format.asciidoc) provides an overview of several different versions of the `cpio` format.

Here we only handle the "New ASCII" (`newc`) variant since that is the most commonly used version.

The archive consists of a sequence of file records.
Each record begins with a 110-byte header,
followed by the file path string (padded with `NUL` so the total length of header + file path is a multiple of 4),
followed by the file content (padded again with `NUL` so the total length of the record is a multiple of 4).

We modify the header of each record as follows:

* UID and GID are set to 0;
* Group and world permissions are removed; owner read and write permissions are added; SUID/SGID/sticky bits are removed;
* Owner execution permission remains untouched;
* Modification time is set to 12:00 AM, 01/01/2000, GMT+0000;
* Device and inode numbers are reassigned sequentially.

This tool is primarily intended for creating reproducible initramfs archives.
This explains how we set the permission bits.
Since all files in initramfs are only going to be used by the root user,
there should be no need for group and world permissions, or SUID/SGID bits.
