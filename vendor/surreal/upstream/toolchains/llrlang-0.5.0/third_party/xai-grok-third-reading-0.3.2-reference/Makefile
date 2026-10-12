.PHONY: check clean

check:
	python3 scripts/run_review.py

clean:
	find . -type d -name __pycache__ -exec rm -rf {} +
	find . -type f -name '*.pyc' -delete
