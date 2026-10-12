SHELL := /bin/bash
.PHONY: all bootstrap check optcheck reverse-verify perf perf-regression clean release manifest

all: bootstrap

bootstrap:
	bash ./scripts/bootstrap.sh

check:
	bash ./scripts/check.sh

optcheck:
	bash ./scripts/optimizer_check.sh

reverse-verify:
	bash ./scripts/reverse_verify.sh

perf:
	bash ./scripts/perf_gate.sh

perf-regression:
	bash ./scripts/paired_perf_gate.sh

manifest:
	bash ./scripts/manifest.sh

release:
	bash ./scripts/release.sh

clean:
	rm -rf build dist
