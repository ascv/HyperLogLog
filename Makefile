.PHONY: help build sdist wheel dist clean install remove check upload upload_test wheel_manylinux wheel_musllinux

PY ?= python3
PACKAGE ?= HLL
CPYTHONS ?= cp39 cp310 cp311 cp312 cp313

help:
	@echo "Targets:"
	@echo "  install         - pip install ."
	@echo "  build           - Build sdist + wheel (host)"
	@echo "  sdist           - Build source distribution only"
	@echo "  wheel           - Build wheel (host, non-portable)"
	@echo "  wheel_manylinux - Build manylinux wheels for CPYTHONS=$(CPYTHONS)"
	@echo "  wheel_musllinux - Build musllinux wheels for CPYTHONS=$(CPYTHONS)"
	@echo "  dist            - Full release: clean + sdist + manylinux + musllinux"
	@echo "  check           - twine check dist/*"
	@echo "  upload          - Upload to PyPI"
	@echo "  upload_test     - Upload to TestPyPI"
	@echo "  remove          - pip uninstall $(PACKAGE)"
	@echo "  clean           - Remove build artifacts"
	@echo ""
	@echo "Variables:"
	@echo "  CPYTHONS - CPython versions for docker wheel targets (default: $(CPYTHONS))"
	@echo ""
	@echo "Requires: pip install build twine"

# ---------- host builds ----------

build:
	@rm -rf build/
	$(PY) -m build

sdist:
	@rm -rf build/
	$(PY) -m build --sdist

wheel:
	@rm -rf build/
	$(PY) -m build --wheel

# ---------- docker wheel builds ----------

wheel_manylinux:
	docker run --rm -v "$$(pwd)":/work -w /work quay.io/pypa/manylinux_2_28_x86_64 \
	  bash -lc ' \
	    set -e; \
	    rm -rf build/; \
	    for tag in $(CPYTHONS); do \
	      pydir=$$(ls -d /opt/python/$${tag}-* 2>/dev/null | head -1) || true; \
	      [ -z "$$pydir" ] && echo "Skipping $$tag (not found)" && continue; \
	      echo "=== $$tag ==="; \
	      $$pydir/bin/pip install -q build && \
	      $$pydir/bin/python -m build --wheel || exit 1; \
	    done; \
	    $$pydir/bin/pip install -q auditwheel; \
	    for whl in dist/*-linux_x86_64.whl; do \
	      [ -f "$$whl" ] || continue; \
	      $$pydir/bin/auditwheel repair "$$whl" -w dist/ && rm "$$whl"; \
	    done \
	  '

wheel_musllinux:
	docker run --rm -v "$$(pwd)":/work -w /work quay.io/pypa/musllinux_1_2_x86_64 \
	  bash -lc ' \
	    set -e; \
	    rm -rf build/; \
	    for tag in $(CPYTHONS); do \
	      pydir=$$(ls -d /opt/python/$${tag}-* 2>/dev/null | head -1) || true; \
	      [ -z "$$pydir" ] && echo "Skipping $$tag (not found)" && continue; \
	      echo "=== $$tag ==="; \
	      $$pydir/bin/pip install -q build && \
	      $$pydir/bin/python -m build --wheel || exit 1; \
	    done; \
	    $$pydir/bin/pip install -q auditwheel; \
	    for whl in dist/*-linux_x86_64.whl; do \
	      [ -f "$$whl" ] || continue; \
	      $$pydir/bin/auditwheel repair "$$whl" -w dist/ && rm "$$whl"; \
	    done \
	  '

# ---------- release ----------

dist: clean sdist wheel_manylinux wheel_musllinux

check:
	$(PY) -m twine check dist/*

upload: check
	@if ls dist/*-linux_x86_64.whl >/dev/null 2>&1; then \
	  echo "ERROR: dist/ contains non-portable linux_x86_64 wheels."; \
	  echo "Run 'make dist' to build manylinux/musllinux wheels."; \
	  exit 1; \
	fi
	$(PY) -m twine upload dist/*

upload_test: check
	@if ls dist/*-linux_x86_64.whl >/dev/null 2>&1; then \
	  echo "ERROR: dist/ contains non-portable linux_x86_64 wheels."; \
	  echo "Run 'make dist' to build manylinux/musllinux wheels."; \
	  exit 1; \
	fi
	$(PY) -m twine upload -r testpypi dist/*

# ---------- development ----------

install:
	pip install .

remove:
	pip uninstall -y $(PACKAGE)

clean:
	rm -rf build/ dist/ *.egg-info/
