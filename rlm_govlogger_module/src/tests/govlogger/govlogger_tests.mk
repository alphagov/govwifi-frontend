GOVLOGGER_TESTS         := base govlogger
GOVLOGGER_OUTPUT	:= $(addsuffix .out,$(addprefix $(BUILD_DIR)/tests/govlogger/,$(GOVLOGGER_TESTS)))
GOVLOGGER_UNIT_BIN	:= $(BUILD_DIR)/bin/local/govlogger_unit
GOVLOGGER_UNIT	:= $(GOVLOGGER_UNIT_BIN)

.PHONY: $(BUILD_DIR)/tests/govlogger/
$(BUILD_DIR)/tests/govlogger/:
	@mkdir -p $@

#
#	Re-run the tests if the test program changes
#
#	Create the output directory before the files
#
$(GOVLOGGER_OUTPUT): $(GOVLOGGER_UNIT_BIN) | $(BUILD_DIR)/tests/govlogger/

#
#	Re-run the tests if the sample output file changes, this also generates the govlogger.output
#       file which is used for the govlogger/govlogger.out check after
#
$(BUILD_DIR)/tests/govlogger/base.out: $(top_srcdir)/src/tests/govlogger/base.out
	@echo GOVLOGGER_TEST $(notdir $@)
	@echo "$(GOVLOGGER_OUTPUT)"
	@rm -f $(BUILD_DIR)/tests/govlogger/govlogger.output $@.stderr $@.stdout
	@$(GOVLOGGER_UNIT) -l $(BUILD_DIR)/tests/govlogger/govlogger.output -t $@ 2>$@.stderr 1>$@.stdout
	@echo "STDOUT" >> $@
	@sed 's/.* : Debug://' $@.stdout >> $@
	@echo "STDERR" >> $@
	@cat $@.stderr >> $@
	@echo GOVLOGGER_TEST checking diffs
	@sed -i 's/ file_rotate_check_ctime set correctly to .*/open_logfile_test file_rotate_check_ctime set correctly to xxxx/' $@
	@sed -i 's/pid [0-9][0-9]*/pid xxxx/' $@
	@if ! diff $< $@; then \
		echo FAILED: " diff $< $@"; \
		echo FAILED: "$(GOVLOGGER_UNIT)"; \
		exit 1; \
	fi;

# After the base.out has been checked we can check the log output
# from previous step - $(BUILD_DIR)/tests/govlogger/govlogger.output

$(BUILD_DIR)/tests/govlogger/govlogger.output.concatenated: $(BUILD_DIR)/tests/govlogger/base.out $(top_srcdir)/src/tests/govlogger/govlogger.out
	@echo GOVLOGGER_TEST Joining govlogger.output and govlogger.output.total into govlogger.output.concatenated
	@cat $(BUILD_DIR)/tests/govlogger/govlogger.output.total $(BUILD_DIR)/tests/govlogger/govlogger.output > $@

$(BUILD_DIR)/tests/govlogger/govlogger.out: $(BUILD_DIR)/tests/govlogger/base.out $(BUILD_DIR)/tests/govlogger/govlogger.output.concatenated
	@echo GOVLOGGER_TEST Checking log output
	@sed -E 's/^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9][0-9]:[0-9][0-9]:[0-9][0-9]\.[0-9]{6}Z //' < $(BUILD_DIR)/tests/govlogger/govlogger.output.concatenated > $@
	@sed -i 's/pid [0-9][0-9]*/pid xxxx/' $@
	@echo GOVLOGGER_TEST Checking output diffs
	@if ! diff $(top_srcdir)/src/tests/govlogger/govlogger.out $@; then \
		echo FAILED: " diff $(top_srcdir)/src/tests/govlogger/govlogger.out $@"; \
		echo FAILED: "$(GOVLOGGER_UNIT) log output"; \
		exit 1; \
	fi;

TESTS.GOVLOGGER_FILES := $(GOVLOGGER_OUTPUT)

$(TESTS.GOVLOGGER_FILES): $(TESTS.UNIT_FILES)

tests.govlogger: $(GOVLOGGER_OUTPUT)
