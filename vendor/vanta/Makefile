.PHONY: bootstrap test cross-seed sanitize security hygiene check cross-check package clean
bootstrap:
	./scripts/bootstrap.sh

test:
	./scripts/run_tests.sh

cross-seed:
	./scripts/check_cross_seed.sh

sanitize:
	./scripts/check_seed_sanitizers.sh

security:
	./scripts/check_binary_security.sh

hygiene:
	./scripts/check_hygiene.sh

check:
	./scripts/release_gate.sh

cross-check:
	python3 scripts/check_crosspiler.py

package:
	./scripts/package.py

clean:
	rm -rf build
