# zat
ZLIB-based data recompression filter for **.zip**, **.slx**, **.mat**, and **.png** files.

## Description
ZAT (**Z**lib **A**rchive **T**ranscoder) uses ZLIB to recompress data streams to a specified compression level. It removes compression applied to  `.zip`, `.slx`, `.mat`, and `.png` files in a Git repository to improve delta detection and reduce storage required.

Because the ZIP format (based on ISO/IEC 21320-1) is widely used across file types (with many differing extensions), ZAT attempts conversion on all matching versioned files.

With compression removed, Git can identify and record only changed data blocks (deltas) instead of whole files. Storing compressed deltas (rather than recompressing original files) reduces repository size and improves incremental storage efficiency for ZIP-container assets, including Microsoft Office documents and MATLAB Simulink files.

**Why this matters:** large binaries can otherwise overwhelm Git storage and slow clone/fetch operations. By removing compression and exposing delta-friendly diffs, ZAT delivers faster repo operations and smaller backups in team workflows.

## Usage

The easiest (and most performant) way to use this feature is to install Git with Zat built-in, which can be enabled with:

```bash
git config zat.enable true
```

If you only have regular Git instead, you can install `git zat` separately and run it before every commit:

```bash
git add changed-files.slx ...
git zat
git commit -m 'Zat my changes'
```

See `pre-commit.sample` to configure a pre-commit hook that automatically zats files in your workspace before committing your changes.

Alternatively, you can use a smudge/clean filter by setting up your `.gitattributes`, e.g.:

```bash
echo '*.slx filter=zat' > .gitattributes
git config filter.zat.clean 'zat -0'
git config filter.zat.smudge zat
```

## Installation

To build and install the executables and the Zat Perl module, run:

```bash
make
sudo make install
```

Included files:
* `git-zat.perl` - Perl script that uses the Zat module to recompress files in the staging area or specified Git references
* `git-zat.adoc` - Documentation for the git-zat utility
* `src/main.c` - Source for the `zat` executable used as a smudge/clean filter
* `zat.adoc` - Documentation for the zat utility
* `pre-commit.sample` - Sample pre-commit hook to automatically zat files before committing

Note: Git with Zat enabled automatically zats files before committing. Use `git-zat` to retroactively zat files in existing repositories and prior commits.

By default, Git with built-in Zat will recompress files when checking them out. You can change this with:
```bash
git config zat.recompress 0 # checkout without any compression
git config zat.recompress 9 # checkout with maximum compression
```

## Author and Copyright

* Written by Hasdi R Hashim <hhashim@ford.com>
* Copyright (C) 2021 by Ford Motor Company
