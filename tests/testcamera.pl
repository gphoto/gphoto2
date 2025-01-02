#!/usr/bin/perl -w
use strict;

$ENV{'LANG'} = 'C';

my $gphoto2 = "gphoto2";

my $debug = 0;

##################################################

# auto configured
my $imageformat;
my $havecapture = 0;
my $havetriggercapture = 0;
my $havepreview = 0;
my $havecapturetarget = 0;
my %formats = ();

# internal

my $debugcnt = 0;

my @lastresult = ();
# returns TRUE on success, FALSE on fail
sub run_gphoto2(@) {
	my @cmdline = @_;

	if ($debug) {
		push @cmdline,"--debug","--debug-logfile=logfile.debug.$debugcnt";
		$debugcnt++;
	}

	@lastresult = ();
	print STDERR "running: " . join(" ",$gphoto2,@ARGV,@cmdline) . "\n";

	print LOGFILE "running: " . join(" ",$gphoto2,@ARGV,@cmdline) . "\n";
	open(GPHOTO,join(" ",$gphoto2,@ARGV,@cmdline)." 2>&1|")||die "open $gphoto2";
	while (<GPHOTO>) {
		print;
		print LOGFILE;
		chomp;
		push @lastresult, $_;
	}
	my $rc = close(GPHOTO);
	print LOGFILE "returned: $rc\n";
	die "camera crashed?" if (grep /No camera found/, @lastresult);
	die "camera crashed?" if (grep /No such device/, @lastresult);
	return $rc;
}

sub ok($$) {
	my ($testres,$text) = @_;

	if ($testres) {
		print "$text: SUCCESS\n";
	} else {
		print "$text: FAIL\n";
	}
}

sub remove_all_files {
	my @files = <*>;

	foreach my $file (@files) {
		next if ($file =~ /logfile/);
		print STDERR "deleting $file\n";
		print LOGFILE "deleting $file\n";
		unlink $file;
	}
}

sub run_gphoto2_capture_target($$@) {
	my ($nrimages,$text,@cmd) = @_;

	ok(&run_gphoto2(@cmd),"$text: " . join(" ",@cmd));

	my @files = <*>;
	if ($nrimages+1 != @files) {
		my $nrfiles = @files;
		print STDERR "*** expected $nrimages files, got " . ($nrfiles-1) . "\n";
		print LOGFILE "*** expected $nrimages files, got " . ($nrfiles-1) . "\n";
	}
	&remove_all_files();
}

sub run_gphoto2_capture($$@) {
	my ($nrimages,$text,@cmd) = @_;
	my @newcmd = @cmd;

	if ($havecapturetarget) {
		my @newcmd = @cmd;
		unshift @newcmd,"--set-config-index","capturetarget=0";
		&run_gphoto2_capture_target($nrimages,$text,@newcmd);

		@newcmd = @cmd;
		unshift @newcmd,"--set-config-index","capturetarget=1";
		&run_gphoto2_capture_target($nrimages,$text,@newcmd);
	} else {
		&run_gphoto2_capture_target($nrimages,$text,@newcmd);
	}
}

sub run_gphoto2_capture_formats($$@) {
	my ($nrimages,$text,@cmd) = @_;

	if (%formats) {
		my @newcmd = @cmd;

		foreach my $format (sort keys %formats) {
			print "testing image $format\n";
			@newcmd = @cmd;
			unshift @newcmd,"--set-config-index",$formats{$format};

			if ($format eq "jpg") {
				&run_gphoto2_capture($nrimages,$text,@newcmd);
			} elsif ($format eq "both") {
				&run_gphoto2_capture($nrimages*2,$text,@newcmd);
				unshift @newcmd,"--keep-raw";
				&run_gphoto2_capture($nrimages,$text,@newcmd);
			} elsif ($format eq "raw") {
				&run_gphoto2_capture($nrimages,$text,@newcmd);
				unshift @newcmd,"--keep-raw";
				&run_gphoto2_capture(0,$text,@newcmd);
			}
		}
		&run_gphoto2("--set-config-index",$formats{'jpg'});
	} else {
		&run_gphoto2_capture($nrimages,$text,@cmd);
	}
}

# START

my $workdir = `mktemp -d /tmp/testcamera.XXXXXX`;
chomp $workdir;
die "no workdir created" unless -d $workdir;
chdir ($workdir);

print "Using temporary directory: $workdir\n";

open(LOGFILE,">logfile.testcase")||die;

if (!run_gphoto2("-L")) {
	print LOGFILE "No camera attached?\n";
	close(LOGFILE);
	die "-L does not work, reason see above\n";
}

# Basics
ok(run_gphoto2("--auto-detect"),"testing --auto-detect");
ok(run_gphoto2("-L"),"testing -L");
ok(run_gphoto2("-l"),"testing -l");

# Autodetect capabilities: capture, preview or trigger capture
ok(run_gphoto2("-a"),"testing -a");
my @abilities = @lastresult;

$havecapture = 1        if (grep (/Capture/,@abilities));
$havetriggercapture = 1 if (grep (/Trigger Capture/,@abilities));
$havepreview = 1        if (grep (/Preview/,@abilities));

ok(run_gphoto2("--summary"),"testing --summary");
ok(run_gphoto2("--list-config"),"testing --list-config");

# Autodetect if some config variables are present

ok(run_gphoto2("--list-all-config"),"testing --list-all-config");
my @allconfig = @lastresult;

$havecapturetarget = 1 if (grep(/capturetarget/,@allconfig));

if (grep(/datetime/,@allconfig)) {
	ok(run_gphoto2("--get-config","datetime"),"testing --get-config datetime before setting");
	ok(run_gphoto2("--set-config","datetime=now"),"testing --set-config datetime=now");
	ok(run_gphoto2("--get-config","datetime"),"testing --get-config datetime after setting");
}
if (grep(/artist/,@allconfig)) {
	ok(run_gphoto2("--get-config","artist"),"testing --get-config artist before setting");
	my $artist;
	foreach (@lastresult) {
		$artist = $1 if (/Current: (.*)/);
	}
	$artist = 0 if ($artist eq "(null)");
	ok(run_gphoto2("--set-config","artist=GPHOTO"),"testing --set-config artist=GPHOTO");
	ok(run_gphoto2("--get-config","artist"),"testing --get-config artist after setting");
	# restore artist
	if ($artist) {
		ok(run_gphoto2("--set-config","artist=$artist"),"testing --set-config artist=$artist");
	}
}

my $inimageformat = 0;
my $jpgformat;
my $rawformat;
my $bothformat;
foreach (@allconfig) {
	if (/^Label:/) {
		#last if ($imageformat);
		$inimageformat = 0;
	}
	if (/^Label: Image Quality 2/) { # Nikon 1
		$imageformat = "imagequality2";
		undef $jpgformat;
		undef $rawformat;
		undef $bothformat;
		$inimageformat = 1;
		next;
	}
	if (/^Label: Image Quality/) {	# Nikon
		last if ($imageformat eq "imagequality2");
		$imageformat = "imagequality";
		$inimageformat = 1;
		next;
	}
	if (/^Label: Image Format/) {	# Canon
		$imageformat = "imageformat";
		$inimageformat = 1;
		next;
	}
	next unless ($inimageformat);

	# save only 1 RAW + JPEG format, it always has a "+" inside
	if (/^Choice: (\d*) .*\+.*/) {
		$bothformat = $1 if (!defined($bothformat));
		next;
	}
	# Take first jpeg format as default
	if (/^Choice: (\d*) .*JPEG/) {
		$jpgformat = $1 if (!defined($jpgformat));
		next;
	}
	# new canon eos style ... use L first
	if (/^Choice: (\d*) .*L$/) {
		$jpgformat = $1 if (!defined($jpgformat));
		next;
	}
	if (/^Choice: (\d*) .*Fine/) {
		$jpgformat = $1 if (!defined($jpgformat));
		next;
	}
	if (/^Choice: (\d*) .*(RAW|NEF)/) {
		$rawformat = $1 if (!defined($rawformat));
		next;
	}
}

if ($imageformat)  {
	die "no jpgformat found in $imageformat" unless (defined($jpgformat));
	$formats{'jpg'} = "$imageformat=$jpgformat";
	print "jpg is $formats{'jpg'}\n";
	print LOGFILE "jpg is $formats{'jpg'}\n";

	if (defined($bothformat)) {
		$formats{'both'} = "$imageformat=$bothformat";
		print LOGFILE "both is $formats{'both'}\n";
		print "both is $formats{'both'}\n";
	} else {
		warn "no bothformat found in $imageformat";
	}

	if (defined($rawformat)) {
		$formats{'raw'} = "$imageformat=$rawformat";
		print LOGFILE "raw is $formats{'raw'}\n";
		print "raw is $formats{'raw'}\n";
	} else {
		warn "no rawformat found in $imageformat" 
	}
	if (!defined($rawformat) && !defined($bothformat)) {
		undef %formats;
	}

} else {
	print "NO imageformat ... just jpeg?\n";
}

# Capture stuff lots of it

if ($havecapture) {
	run_gphoto2_capture_formats(1,"simple capture and download","--capture-image-and-download");
	run_gphoto2_capture_formats(3,"multiframe capture and download","--capture-image-and-download","-F 3","-I 3");
	run_gphoto2_capture_formats(1,"waitevent","--wait-event-and-download=10s");
}
if ($havetriggercapture) {
	run_gphoto2_capture_formats(1,"trigger capture","--trigger-capture","--wait-event-and-download=5s");
}
if (grep (/eosremoterelease/,@allconfig)) {
	run_gphoto2_capture(1,"eos remote release","--set-config","eosremoterelease=Immediate","--wait-event=2s","--set-config","eosremoterelease='Release Full'","--wait-event-and-download=5s");
}

if (grep (/eosremoterelease/,@allconfig)) {
	run_gphoto2_capture(1,"eos remote release","--set-config","eosremoterelease=Immediate","--wait-event-and-download=2s","--set-config","eosremoterelease='Release Full'","--wait-event-and-download=5s");
}

if ($havepreview) {
	ok(run_gphoto2("--capture-preview"),"testing --capture-preview");
	remove_all_files();
	ok(run_gphoto2("--capture-preview"),"testing --capture-preview 2nd time");
	remove_all_files();
	ok(run_gphoto2("--capture-movie=5s"),"testing --capture-movie=5s");
	remove_all_files();
}
