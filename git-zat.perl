#!/usr/bin/env perl
######################################################################
#
# Written by Hasdi R Hashim <hhashim@ford.com>
#
######################################################################
# git-zat: Apply zat (ZLIB recompression) to git repository blobs
# Removes compression from .zip, .mat, and .png files in Git history
#
# Usage: git zat [--sdbm[=<path>]] [<refspec>...]
#   If no refspecs: zat files in staging area
#   If refspecs:    zat files in specified branches/tags

use strict;
use IPC::Open2;              # For two-way pipe communication with git
use Digest::SHA qw(sha1_hex);
use Zat;                      # C extension for zatting blobs
use Compress::Raw::Zlib;

# Process handle for persistent git cat-file --batch stream
my $git_pid = undef;

# Find the root of the git repository
chomp(my $git_dir = qx(git rev-parse --show-toplevel));

# Caches to avoid re-fetching objects from git
# Keys: object SHA-1 hashes, Values: object data structures
my %cache_blob;      # Cache for blob objects (file contents)
my %cache_tree;      # Cache for tree objects (directory listings)
my %cache_commit;    # Cache for commit objects
my %cache_tag;       # Cache for tag objects

# Fetch all refs (branches and tags) from git repository
my @refs = qx"git for-each-ref  --format '%(if)%(symref)%(then)%(else)%(objectname)\t%(objecttype)\t%(refname)%(end)'";
scalar @refs || die "ERROR: unable to get any branches or tags\n";

# Regex pattern for valid reference names (refspec components)
my $REF = '[^:^~ *]+';

# Maps for reference resolution
my %target;   # refspec destinations -> source refs (for applying changes)
my %ref;      # ref names -> object SHA-1 hashes
my %type;     # object SHAs -> type (commit, tag, etc.)

# For future expansion
my %local;
my %remotes;

# Optional SDBM cache for performance: tracks previously zatted objects
my %database;

# Parse git refs output and populate lookup tables
foreach (@refs) {
	/(\S+)\t(\S+)\t(.*)/ || next;
	my ($id,$type,$name) = ($1,$2,$3);
	$ref{$name} = $id;         # Map ref name to its object hash
	$type{$id} = $type;        # Map object hash to its type (commit/tag)
}

use Fcntl;      # For O_RDWR, O_CREAT, etc. (file opening flags)
use SDBM_File;  # For persistent object hash cache

my @refspecs;

foreach (@ARGV) {
	if
	(/^--sdbm$/) {
		tie %database, 'SDBM_File', $git_dir.'/.git/zat', O_RDWR|O_CREAT, 0666 || die;
	} elsif
	(/^--sdbm=(.+)$/) {
		my $path = $1;
		# Sanitize path: reject null bytes, absolute paths, and path traversal
		if ($path =~ /\0/ || $path =~ /^\// || $path =~ /\.\./) {
			die "ERROR: invalid path for --sdbm: $path\n";
		}
		tie %database, 'SDBM_File', $path, O_RDWR|O_CREAT, 0666 || die;
	} elsif
	(/^-h$/ || /^--help$/) {
		print "usage:\tgit zat [--sdbm[=<path>]] [<refspec>...]\n";
		exit;
	} elsif
	(/^-/) {
		# Unknown option
		die "ERROR: unknown option: $_\nusage:\tgit zat [--sdbm[=<path>]] [<refspec>...]\n";
	} else {
		# Validate refspec format
		unless (/^\+(${REF})(?::(${REF})?)?\*?$/ || /^\+(${REF})\*(?::(${REF})\*)?$/) {
			die "ERROR: invalid refspec: $_\nExpected format: +<ref> or +<ref>:<ref> with optional * wildcards\n";
		}
		push @refspecs, $_;
	}
}

# If no refspecs provided, zat files in the staging area
unless (scalar @refspecs) {
	my %entries;  # Track changed entries to update index

	foreach (qx|git ls-files --stage|) {
		my ($mode, $id, $stage, $path) = split;
		next if oct($mode) eq 0160000;  # Skip gitlinks (submodules)

		# Process blob through zat
		my ($new_id,@data) = zat_blob($id);
		next if $new_id eq $id;  # Skip if zat made no changes

		# Track the change for git index update
		$entries{$path} = "$mode $new_id $stage\t$path\n";

		# Write zatted data back to working directory
		open OUT,">$path" or die;
		print OUT @data;
		close OUT;
		printf STDOUT "ZAT\t$path\n";
	}

	# If any files were zatted, update git index
	if (scalar %entries) {
		open OUT, '|git update-index --index-info' or die;
		print OUT values %entries;
		close OUT;
	}
	exit;
}

foreach (@refspecs) {
	if
	(/^\+(${REF})$/) {
		# Simple add: +refs/heads/main
		$target{$1} = $1 if exists $ref{$1};
	} elsif
	(/^\+(${REF}):$/) {
		# Delete: +refs/heads/main: (remove from target)
		delete $target{$1} if exists $ref{$1};
	} elsif
	(/^\+(${REF}):(${REF})$/) {
		# Rename/move: +refs/heads/old:refs/heads/new
		$target{$2} = $1 if exists $ref{$1}
	} elsif
	(/^\+(${REF})\*$/) {
		# Wildcard add: +refs/heads/* (match all branches)
		my $src = $1;
		foreach my $name (keys %ref) {
			$target{"$src$1"} = $name if $name =~ /^${src}(${REF})$/;
		}
	} elsif
	(/^\+(${REF})\*:$/) {
		# Wildcard delete: +refs/heads/*:
		my $src = $1;
		foreach my $name (keys %ref) {
			delete $target{"$src$1"} if $name =~ /^${src}(${REF})$/;
		}
	} elsif
	(/^\+(${REF})\*:(${REF})\*$/) {
		# Wildcard rename: +refs/remotes/src/*:refs/heads/local/*
		my ($src,$tgt) = ($1,$2);
		foreach my $name (keys %ref) {
			$target{"$tgt$1"} = $name if $name =~ /^${src}(${REF})$/;
		}
	} else {
		# Invalid refspec
		die "ERROR: unrecognized refspec pattern: '$_'\nValid formats: +<ref>, +<ref>:, +<src>:<dst>, +<src>*, +<src>*:, +<src>*:<dst>*\n";
	}
}

unless (scalar %target) {
	print "Unable to find matching references:\n";
	print "\t$_\n" foreach @refspecs;
	exit;
}

# Collect all commits to be rewritten
my @commits;

# Build topologically-sorted list of all commits to process
foreach my $name(sort values %target) {
	next if not exists $ref{$name};
	my $id = $ref{$name};
	my $type = $type{$id};

	if ($type eq 'commit') {
		# Follow commit's ancestry tree
		git_commit($id,\@commits);
	} elsif ($type eq 'tag') {
		# Extract commit from tag and process its ancestry
		($id) = git_tag($id);
		git_commit($id,\@commits);
	} else { die "$id\t$type\t$name"; }
}

# Process each commit, rewriting its tree and parents if needed
my $count = 0;
my $time_start = time;
my $elapsed = 0;

my $total = scalar @commits;

foreach my $commit(@commits) {
	# Recursively zat the commit's tree and update parent references
	zat_commit($commit);
	++$count;

	# Display progress estimate (update only once per second)
	my $lapse = time - $time_start;
	next if $lapse == $elapsed && $count < $total;
	$elapsed = $lapse;

	my $remaining = int(($total - $count) * $elapsed / $count);
	printf STDOUT "\rRevising commit $commit ($count/$total) ($elapsed seconds, about $remaining remaining)    ";
	flush STDOUT;
}

# Update all refs to point to their new (zatted) commits
printf STDOUT "\nUpdating references... ";
flush STDOUT;

my @commands = ();  # Commands for git update-ref --stdin

# Build git update-ref commands for all changed references
foreach my $dst(keys %target) {
	my $name = $target{$dst};
	if (! defined $name) {
		die unless exists $ref{$dst}; # ASSERT
		push @commands, "delete $dst\n";
	} else {
		my $id = $ref{$name};
		my $type = $type{$id};

		if ($type eq 'commit') {
			$id = $database{$id} if exists $database{$id} && $database{$id};
		} elsif ($type eq 'tag') {
			my ($commit,$data) = git_tag($id);
			if (exists $database{$commit} && $database{$commit}) {
				$data =~ s/^object $commit/object $database{$commit}/ || die;
				if ($data =~ s/\n-----BEGIN PGP SIGNATURE-----.*//s) {
					print STDERR "WARNING: tag $id stripped of PGP signature\n";
				}
				$id = $database{$id} = writ_object(pack_object('tag',$data));
			}
		}
		if (defined $ref{$dst}) {
			push @commands, "update $dst $id ".($ref{$dst})."\n" if $id ne $ref{$dst};
		} else {
			push @commands, "create $dst $id\n";
		}
	}
}

@commands = sort @commands;

if (scalar @commands) {
	open UPDATE,"|git update-ref --stdin" || die;
	print UPDATE @commands;
	close UPDATE;

	printf STDOUT "%i changed.\n", scalar @commands;
} else {
	printf STDOUT "no changes.\n";
}

exit;

# Subroutines for Git object manipulation and zatting

# writ_object: Compress and store a git object in the object database
# Arguments:
#   @_: Object content parts (will be concatenated and compressed)
# Returns:
#   Object SHA-1 hash (also used as the object ID)
sub writ_object {
	my $id = sha1_hex(@_);  # Compute SHA-1 of full object content

	# Git object database uses 2-char subdirectory structure
	my $path = ($git_dir).'/.git/objects/'.(substr($id,0,2)).'/';
	mkdir $path;  # Create 2-char subdirectory if needed

	my $file = $path.(substr($id,2));  # Full object file path

	return $id if -s $file;  # Object already exists, return its hash

	# Write new object to file
	open (OUT, '>', $file ) or die "open(OUT,'>$path'): $!";

	# Compress content using zlib deflate
	die "new Deflate()" unless my $defl = new Compress::Raw::Zlib::Deflate;

	# Deflate each content part
	foreach (@_) {
		$defl->deflate($_,my $buf);
		print OUT $buf;
	}
	# Flush final compressed bytes
	$defl->flush(my $end);
	print OUT $end;

	close OUT;

	return $id;  # Return the object hash
}

# git_object: Fetch raw object data from git repository
# Arguments:
#   $object_id: SHA-1 hash of the object to retrieve
# Returns:
#   ($type, @data): Object type and content chunks
# Note:
#   Uses persistent 'git cat-file --batch' pipe for efficiency
sub git_object {
	my $object_id = shift;

	# Lazy-initialize persistent git cat-file connection
	if ($git_pid eq undef) {
		$git_pid = open2(\*GIT_OUT, \*GIT_IN, 'git', 'cat-file', '--batch')
			or die "open2(GIT_OUT,GIT_IN,'git cat-file --batch'";
	}
	print GIT_IN "$object_id\n";	# Request object from git
	$_ = readline GIT_OUT;

	unless (/^(\S+) (\S+) (\d+)\n/) {
		return () if / missing\n$/;       # Object not found
		return (undef, $_);               # Parse error
	}
	my ($sha1, $type, $size) = ($1,$2,$3);

	unless ( $sha1 eq $object_id) {
		system 'git ls-files --stage';
		die "ERROR: git cat-file '$object_id' --> '$_'\n";
	}

	# Read object content chunks
	my @data = ( $type );  # First element is always the type
	while ($size) {
		my $n = read GIT_OUT, my $output, $size;
		die "read()" unless defined $n;
		last unless $n;
		$size = $size - $n;
		push @data, $output;  # Accumulate content chunks
	}
	die "ERROR: '$object_id' reading incomplete ($size bytes remained)" if $size;

	# Consume null terminator
	read GIT_OUT, my $newline, 1;

	return @data;
}

# git_blob: Fetch and cache a blob (file) object
# Arguments:
#   $query: SHA-1 hash of the blob
# Returns:
#   @data: Blob content chunks
sub git_blob {
	my $query = shift;
	if (exists $cache_blob{$query}) {
		return @{$cache_blob{$query}};  # Return cached blob
	}

	my ($type,@data) = git_object($query);
	return () unless defined $type;
	die unless $type eq "blob";  # Verify object is a blob

	$cache_blob{$query} =  \@data;  # Cache for future lookups

	return @data;
}

# git_tree: Fetch and parse tree (directory listing) object
# Arguments:
#   $query: SHA-1 hash of the tree
# Returns:
#   @files: Array of file entries [ mode, name, object-id ]
sub git_tree {
	my $query = shift;
	if (exists $cache_tree{$query}) {
		return @{$cache_tree{$query}};  # Return cached tree
	}

	my ($type,@data) = git_object($query);
	return () unless defined $type;
	die unless $type eq "tree";
	my $data = join('',@data);  # Concatenate all content chunks

	# Parse git tree format: mode name\0object_sha1
	my @files = ();
	while ($data =~ /\G(\d+)\s+([^\000]+)\000(.{20})/gs ) {
		push @files, [ $1, $2, unpack("H*", $3) ];  # mode, name, object-id (hex)
	}
	$cache_tree{$query} =  \@files;  # Cache for future lookups

	return @files;
}

# git_commit: Fetch, parse, and cache commit object
# Arguments:
#   $query: SHA-1 hash of the commit
#   $list : Reference to array to accumulate commits (optional, for topological sort)
# Returns:
#   ($tree, \@parents, $raw_data): Tree hash, parent list, raw commit data
sub git_commit {
	my ($query, $list) = @_;
	if (exists $cache_commit{$query}) {
		return @{$cache_commit{$query}};  # Return cached commit
	}

	my ($type,@data) = git_object($query);
	return () unless defined $type;
	die unless $type eq "commit";  # Verify object is a commit
	my $data = join('',@data);  # Concatenate all content chunks

	my ($tree,@parents,@author,@committer,$message);
	my $n = 0;

	# Parse commit metadata (tree, parents, author, committer)
	while (my $l = index($data,"\n",$n)) {
		if ($l == $n) {
			$message = substr($data,$n+1);
			last;
		}
		$_ = substr($data,$n,$l-$n);
		if (/tree (\S+)/)		{	$tree = $1;					}
		elsif (/parent (\S+)/)	{	push @parents,$1;			}
		elsif (/author ([^<]*)\s+<(.+)>\s+(\d+\s+.\d+)/)
								{	@author = ($1,$2,$3);		}
		elsif (/committer ([^<]*)\s+<(.+)>\s+(\d+\s+.\d+)/)
								{	@committer = ($1,$2,$3);	}
		else {
			die "commit-parse: $_\n";
		}
		$n = $l + 1;
	}

	my $ref = [ $tree,\@parents,$data ];
	$cache_commit{$query} = $ref;  # Cache for future lookups

	# Recursively process all parent commits (builds topological order)
	foreach my $parent(@parents) {
		git_commit($parent,$list);
	}
	# Add this commit to the processing list
	push @{$list},$query if $list;
	return @{$ref};
}

# git_tag: Fetch and parse annotated tag object
# Arguments:
#   $query: SHA-1 hash of the tag
# Returns:
#   ($commit_id, $raw_data): Commit hash pointed to, raw tag data
sub git_tag {
	my $query = shift;
	if (exists $cache_tag{$query}) {
		return @{$cache_tag{$query}};  # Return cached tag
	}

	my ($type,@data) = git_object($query);
	die unless $type eq "tag";

	my $data = join('',@data);  # Concatenate all content chunks
	# Extract commit hash from tag metadata
	$data =~ /^object (\S+)\ntype commit\n/ || die;
	my $commit = $1;

	my $ref = [ $commit, $data ];
	$cache_tag{$query} = $ref;  # Cache for future lookups

	return @{$ref};
}

# pack_object: Create git object with proper header format
# Arguments:
#   $type: Object type (blob, tree, commit, tag)
#   @_ : Object content chunks
# Returns:
#   @: Header and content chunks ready for storage
sub pack_object {
	my $type = shift;
	my $size = 0; $size += length foreach (@_);  # Calculate total size
	return ("$type $size\0", @_);  # Git object format: type size\0data
}

# pack_tree: Assemble tree object from file entries
# Arguments:
#   @_: Array of [ mode, name, object-id ] entries
# Returns:
#   @: Git object content (header + tree data)
sub pack_tree {
	my @files;
	foreach (@_) {
		my ($mode,$name,$object_id) = @{$_};
		# Tree format: mode name\0object_sha1 (binary)
		my $entry = "$mode $name\0".pack("H*", $object_id);
		push @files,$entry;
	}
	return pack_object("tree", @files);  # Wrap in object header
}

# zat_blob: Apply zat compression removal to a blob object
# Arguments:
#   $query: SHA-1 hash of the blob
# Returns:
#   ($new_id, @data): New object hash (if changed), blob content
# Note:
#   Uses %database cache to track which blobs were already zatted
sub zat_blob {
	my $query = shift;
	# Check if blob was already processed
	if (exists $database{$query}) {
		my $updated_id = $database{$query};
		if ($updated_id) {
			# Blob was zatted, return new version
			my @data = git_blob($updated_id);
			return ( $updated_id, @data ) if scalar @data;
		} else {
			# Blob was checked, not changed
			return ( $query, git_blob($query) );
		}
	}
	# Fetch original blob and apply zat
	my @data = git_blob($query);
	my ($status, @zdat) = zat(0, @data);  # C extension call
	if ($status == 0) {
		# Zat succeeded, check if content actually changed
		my @blob = pack_object("blob",@zdat);
		if (sha1_hex(@blob) ne $query) {
			# Content changed, store new blob and update database
			my $newID = writ_object(@blob);
			$database{$query} = $newID;   # Mark original as zatted
			$database{$newID} = '';       # Mark result as final
			$cache_blob{$newID} = \@zdat;  # Cache new blob
			return ( $newID, @zdat );
		}
	}
	# Zat didn't change blob
	$database{$query} = '';  # Mark as checked but unchanged
	return ( $query, @data );
}

# zat_tree: Recursively apply zat to all blobs in tree
# Arguments:
#   $query: SHA-1 hash of the tree
# Returns:
#   ($new_id, @entries): New tree hash (if changed), file entries
# Note:
#   Recursively processes subdirectories and zatts blobs
sub zat_tree {
	my $query = shift;
	# Check if tree was already processed
	if (exists $database{$query}) {
		my $updated_id = $database{$query};
		if ($updated_id) {
			# Tree was changed, return new version
			my @data = git_tree($updated_id);
			return ( $updated_id, @data ) if scalar @data;
		} else {
			# Tree was checked, not changed
			return ( $query, git_tree($query) );
		}
	}
	# Fetch tree entries and recursively process each
	my @files = git_tree($query);
	my @zfiles = ();
	my $changed = 0;
	foreach (@files) {
		my ($mode,$imod,$name,$old_file) = ( $$_[0], oct($$_[0]), $$_[1], $$_[2] );
		# Recursively process based on file type
		my ($new_file) =
			$imod == 0040000 ? zat_tree($old_file) :        # Subdirectory
			$imod != 0160000 ? zat_blob($old_file) :        # Regular file
			$old_file;                                       # Gitlink (skip)
		$changed = 1 if $new_file ne $old_file;  # Track if any changes
		push @zfiles, [ $mode, $name, $new_file ];
	}

	if (! $changed) {
		# No changes in this tree
		$database{$query} = '';  # Mark as checked
		return ( $query , @files );
	}
	# Tree changed, create new tree object
	my $newID = writ_object( pack_tree(@zfiles) );
	$cache_tree{$newID} = \@zfiles;  # Cache new tree
	$database{$query} = $newID;      # Mark original as zatted
	$database{$newID} = '';          # Mark result as final
	return ( $newID, @zfiles );
}

# zat_commit: Rewrite commit with zatted tree and updated parents
# Arguments:
#   $query: SHA-1 hash of the commit
#   $list : Reference to commit list for processing order
# Returns:
#   ($new_id, $tree, \@parents, $raw_data): New commit details
# Note:
#   Recursively zatts tree and updates parent references if parents changed
sub zat_commit {
	my ($query, $list) = @_;
	# Check if commit was already processed
	if (exists $database{$query}) {
		my $updated_id = $database{$query};
		if ($updated_id) {
			# Commit was rewritten, return new version
			my @data = git_commit($updated_id, $list);
			return ( $updated_id, @data ) if scalar @data;
		} else {
			# Commit was checked, not changed
			return ( $query, git_commit($query, $list) );
		}
	}

	# Fetch original commit and process its tree
	my ($tree,$parents,$data) = git_commit($query, $list);
	my $ztree = (zat_tree($tree))[0];  # Recursively zat the tree
	my $changed = $tree ne $ztree ? $data =~ s/tree $tree\n/tree $ztree\n/ : 0;  # Update tree reference

	# Process parent commits (may have been rewritten)
	my @zparents;
	foreach my $old (@{$parents}) {
		# Check database cache first, then recursively process
		my $new = exists $database{$old} ? $database{$old} : (zat_commit($old, $list))[0];

		if (! $new || $new eq $old) {
			# Parent unchanged
			push @zparents,$old;
			next;
		}
		$changed = 1;
		if (grep /^$new$/, @zparents) {
			# Parent already in list (merge commit deduplication)
			$data =~ s/parent $old\n// || die;
			next;
		}
		push @zparents,$new;
		$data =~ s/parent $old\n/parent $new\n/ || die;  # Update parent reference
	}

	if (! $changed) {
		# No changes to commit
		$database{$query} = '';  # Mark as checked
		return ( $query, $tree, $parents, $data );
	}
	# Commit changed, create new commit object
	my $ref = [ $ztree,\@zparents,$data ];
	my $newID = writ_object( pack_object("commit",$data) );
	$database{$query} = $newID;      # Mark original as rewritten
	$database{$newID} = '';          # Mark result as final
	$cache_commit{$newID} = $ref;    # Cache new commit
	push @{$list},$newID if $list;   # Add to processing list
	return ( $newID, @{$ref} );
}
