SUBDIRS = data staging

EXTRA_DIST = TESTING testsuite testcamera.pl

TESTS = testsuite

TESTS_ENVIRONMENT = env

# Simulate the "testsuite" script being called with "-v".
TESTS_ENVIRONMENT += VERBOSECOMMAND='set -x ;'

clean-local:
	-rm -rf log
