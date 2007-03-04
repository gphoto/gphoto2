#!/usr/bin/perl
#
# Uploads MP3s from current directory, recursively.
# Optional arguments can be directories.
# 
use File::Basename;
use MP3::Info qw(:DEFAULT :utf8);

if ($ENV{'LANG'} =~ /utf-8/i) {
	use_mp3_utf8();
}

foreach $dir (@ARGV) {
	push @dirs,`find $dir -type d`;
}
if ($#dirs < 0) {
	@dirs = `find . -type d`;
}
chomp @dirs;


#AudioWAVECodec Track OriginalReleaseDate SampleRate AudioBitDepth

sub escape {
	my ($txt) = @_;

	return $txt;
}

sub dump_tags  {
	my ($file) = @_;
	my $key;
	my $metafn;
	my $tinfo = get_mp3tag($file);
	my %tmap = %{$tinfo};

	#foreach $key (keys %tmap) { print STDERR "$key -> " . $tmap{$key} . "\n"; }
	my $finfo = get_mp3info($file);
	my %fmap = %{$finfo};
	#foreach $key (keys %fmap) { print STDERR "$key -> " . $fmap{$key} . "\n"; }

	$metafn = $file;
	$metafn =~ s/.*\/([^\/]*)$/meta_$1/;
	open(META,">$metafn") || die "$metafn:$!\n";
	if (defined($tmap{TITLE}) && $tmap{TITLE} ne "") {
		print META "<Name>" . escape($tmap{TITLE}) . "</Name>\n";
	}
	if (defined($tmap{ARTIST}) && $tmap{ARTIST} ne "") {
		print META "<Artist>" . escape($tmap{ARTIST}) . "</Artist>\n";
	}
	if (defined($tmap{ALBUM}) && $tmap{ALBUM} ne "") {
		print META "<AlbumName>" . escape($tmap{ALBUM}) . "</AlbumName>\n";
	}
	if (defined($tmap{GENRE}) && $tmap{GENRE} ne "") {
		print META "<Genre>" . escape($tmap{GENRE}) . "</Genre>\n";
	}
	if ($tmap{STEREO} == 1) {
		print META "<NumberOfChannels>2</NumberOfChannels>\n";
	} elsif ($tmap{STEREO} == 0) {
		print META "<NumberOfChannels>1</NumberOfChannels>\n";
	} else {
		# default stereo
		print META "<NumberOfChannels>2</NumberOfChannels>\n";
	}
	if (defined($fmap{SECS})) {
		# in milliseconds
		printf META "<Duration>%d</Duration>\n",$fmap{SECS}*1000;
	}
	if (defined($fmap{BITRATE})) {
		printf META "<AudioBitRate>%d</AudioBitRate>\n",$fmap{BITRATE};
	}
	if (defined($fmap{FREQUENCY})) {
		printf META "<SampleRate>%d</SampleRate>\n",$fmap{FREQUENCY}*1000;
	}
	close(META);
	return $metafn;
}

%seendir = ();
foreach $dir (@dirs) {
	$dir =~ s/^\.\///;
	@files = `find '$dir' -maxdepth 1 -name "*.mp3"`;
	chomp @files;
	next unless (@files > 0);

	@dircomp = split(/\//,$dir);

	$xdir = "";
	foreach $subdir (@dircomp) {
		$parentdir = $xdir;
		if ($xdir eq "") {
			$xdir .= "$subdir";
		} else {
			$xdir .= "/$subdir";
		}
		next if ($seendir{$xdir});
		system("gphoto2 -f '/store_00010001/Music/$parentdir' --mkdir '$subdir'");
		# print "gphoto2 -f '/store_00010001/Music/$parentdir' --mkdir '$subdir'\n";
		$seendir{$xdir} = 1;

	}
	$uploadline = " -f '/store_00010001/Music/$dir'";
	foreach $file (@files) {
		$metafn = dump_tags($file);
		$uploadline .= " -u \'" . $file . "\'";
		if ($metafn) {
			push @metas, $metafn;
			$uploadline .= " --upload-metadata \'" . $metafn . "\'";
		}
	}
	system("gphoto2 $uploadline");
	unlink(@metas);
}
