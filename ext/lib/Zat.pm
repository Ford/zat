package Zat;

use 5.032000;
use strict;
use warnings;

require Exporter;
	our @ISA = qw(Exporter);
	our @EXPORT = qw(zat);
	our $VERSION = '1.00';

require XSLoader;
	XSLoader::load('Zat', $VERSION);

# Preloaded methods go here.

1;
__END__

=head1 NAME

Zat - A perl utility module for ZLIB-based recompression

=head1 SYNOPSIS

	use Zat;
	use Compress::Zlib qw(Z_DEFAULT_COMPRESSION Z_OK)

	my ($status,@output) = zat(Z_DEFAULT_COMPRESSION,<STDOUT>);
	print STDOUT @output if $status == Z_OK;

=head1 DESCRIPTION
	The 'zat' command uses ZLIB library to recompress an input array (with a specified compression-level)
	to an output array. If successful, it returns a status code of Z_OK. Otherwise, it would be Z_DATA_ERROR
	or other ZLIB error codes.

=head1 AUTHOR AND COPYRIGHT

Written by Hasdi R Hashim <hhashim@ford.com>
Copyright (C) 2021 by Ford Motor Company

=cut
