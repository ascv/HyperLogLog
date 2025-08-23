.PHONY: help build dist sdist wheel clean install remove check upload upload_test wheel_manylinux wheel_musllinux

PY ?= python3
PACKAGE ?= HLL

help:
	@echo "Targets:"
	@echo "  build           - Build sdist+wheel on host (may produce linux_x86_64 wheel)"
	@echo "  sdist           - Build source distribution only"
	@echo "  wheel           - Build wheel only (host)"
	@echo "  wheel_manylinux - Build inside manylinux & repair wheel (recommended for PyPI)"
	@echo "  wheel_musllinux - Build inside musllinux (Alpine/musl) & repair"
	@echo "  check           - twine check dist/*"
	@echo "  upload          - Upload manylinux/musllinux wheel + sdist if present; else sdist only"
	@echo "  upload_test     - Upload to TestPyPI"
	@echo "  install         - pip install ."
	@echo "  remove          - pip uninstall $(PACKAGE)"
	@echo "  clean           - Remove build artifacts"

build:
	$(PY) -m pip install -U build
	$(PY) -m build

sdist:
	$(PY) -m pip install -U build
	$(PY) -m build --sdist

wheel:
	$(PY) -m pip install -U build
	$(PY) -m build --wheel

clean:
	rm -rf build/ dist/ *.egg-info/ .pytest_cache/ .venv/

install:
	pip install .

remove:
	pip uninstall -y $(PACKAGE)

check:
	$(PY) -m pip install -U twine
	twine check dist/*

wheel_manylinux:
	docker run --rm -v "$$(pwd)":/work -w /work quay.io/pypa/manylinux_2_28_x86_64 \
	  bash -lc '\
	    python3.12 -m venv .venv && source .venv/bin/activate && \
	    python -m pip install -U pip build auditwheel && \
	    python -m build --sdist --wheel && \
	    auditwheel repair dist/*-linux_x86_64.whl -w dist/ \
	  '

wheel_musllinux:
	docker run --rm -v "$$(pwd)":/work -w /work quay.io/pypa/musllinux_1_2_x86_64 \
	  bash -lc '\
	    python3.12 -m venv .venv && source .venv/bin/activate && \
	    python -m pip install -U pip build auditwheel && \
	    python -m build --sdist --wheel && \
	    auditwheel repair dist/*-linux_x86_64.whl -w dist/ \
	  '

upload: check
	@echo "Uploading wheels (manylinux/musllinux) + sdist if available; else sdist only…"
	@if ls dist/*manylinux*.whl >/dev/null 2>&1 || ls dist/*musllinux*.whl >/dev/null 2>&1; then \
	  twine upload dist/*manylinux*.whl dist/*musllinux*.whl dist/*.tar.gz; \
	else \
	  echo "No compliant wheel found; uploading sdist only."; \
	  twine upload dist/*.tar.gz; \
	fi

upload_test: check
	@if ls dist/*manylinux*.whl >/dev/null 2>&1 || ls dist/*musllinux*.whl >/dev/null 2>&1; then \
	  twine upload -r testpypi dist/*manylinux*.whl dist/*musllinux*.whl dist/*.tar.gz; \
	else \
	  echo "No compliant wheel found; uploading sdist only to TestPyPI."; \
	  twine upload -r testpypi dist/*.tar.gz; \
	fi

