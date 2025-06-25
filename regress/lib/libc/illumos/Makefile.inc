ILLUMOS_OS_TESTDIR = /usr/local/share/illumos-os-tests

.if !exists(${ILLUMOS_OS_TESTDIR})
regress:
	@echo package illumos-os-tests is required for this regress
	@echo SKIPPED
.endif
